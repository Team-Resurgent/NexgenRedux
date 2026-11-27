#include "net/secure.h"
#include "base/log.h"
#include "base/memory.h"
#include "base/thread.h"
#include "platform/platform.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_time.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/ssl.h>
#include <mbedtls/threading.h>
#include <psa/crypto.h>

#include <stdio.h>
#include <string.h>
#include <string>
#include <time.h>

#if defined(_XBOX)
#include <xtl.h>
#include <winsockx.h>
#elif defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

namespace nx
{
    namespace
    {
        Platform* g_platform = nullptr;
        Mutex g_lock;
        bool g_ready = false, g_failed = false;
        mbedtls_x509_crt g_trusted;
        mbedtls_entropy_context g_entropy;
        mbedtls_ctr_drbg_context g_random;
        mbedtls_ssl_config g_config;

        void* zeroed(size_t count, size_t size)
        {
            if (size && count > ~static_cast<size_t>(0) / size)
            {
                return nullptr;
            }
            const size_t bytes = count * size;
            void* p = memAlloc(bytes ? bytes : 1, MemTag::General);
            if (p)
            {
                memset(p, 0, bytes);
            }
            return p;
        }

        // Seconds since 1970, or 0 if the clock can't be right.
        i64 clockSeconds()
        {
#if defined(_XBOX) || defined(_WIN32)
            FILETIME f;
            GetSystemTimeAsFileTime(&f);
            const u64 t = static_cast<u64>(f.dwHighDateTime) << 32 | f.dwLowDateTime;
            const i64 seconds = static_cast<i64>(t / 10000000) - 11644473600ll;
#else
            const i64 seconds = static_cast<i64>(::time(nullptr));
#endif
            constexpr i64 k2026 = 1767225600;
            return seconds < k2026 ? 0 : seconds;
        }

        mbedtls_time_t secureTime(mbedtls_time_t* out)
        {
            const mbedtls_time_t now = static_cast<mbedtls_time_t>(clockSeconds());
            if (out)
            {
                *out = now;
            }
            return now;
        }

        void mutexInit(mbedtls_threading_mutex_t* m) { m->lock = create<Mutex>(MemTag::General); }
        void mutexFree(mbedtls_threading_mutex_t* m)
        {
            destroy(static_cast<Mutex*>(m->lock));
            m->lock = nullptr;
        }
        int mutexLock(mbedtls_threading_mutex_t* m)
        {
            if (m->lock == nullptr)
            {
                return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
            }
            static_cast<Mutex*>(m->lock)->lock();
            return 0;
        }
        int mutexUnlock(mbedtls_threading_mutex_t* m)
        {
            if (m->lock == nullptr)
            {
                return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
            }
            static_cast<Mutex*>(m->lock)->unlock();
            return 0;
        }

        // A console whose clock has stopped can't tell what is out of date. Everything else is still checked.
        int checked(void*, mbedtls_x509_crt*, int, uint32_t* flags)
        {
            if (clockSeconds() == 0)
            {
                *flags &= ~static_cast<uint32_t>(MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE);
            }
            return 0;
        }

        bool prepare()
        {
            Lock lock(g_lock);
            if (g_ready || g_failed)
            {
                return g_ready;
            }
            g_failed = true;
            mbedtls_platform_set_calloc_free(zeroed, memFree);
            mbedtls_platform_set_time(secureTime);
            mbedtls_threading_set_alt(mutexInit, mutexFree, mutexLock, mutexUnlock);
            mbedtls_x509_crt_init(&g_trusted);
            mbedtls_entropy_init(&g_entropy);
            mbedtls_ctr_drbg_init(&g_random);
            mbedtls_ssl_config_init(&g_config);
            FileData file;
            if (g_platform == nullptr || !g_platform->readFile("global/certificates.pem", file, MemTag::General))
            {
                NX_ERROR("global/certificates.pem cannot be read, so nothing secure can be reached");
                return false;
            }
            // It is read as text, with an end.
            std::string text(reinterpret_cast<const char*>(file.data), file.size);
            file.clear();
            if (mbedtls_x509_crt_parse(&g_trusted, reinterpret_cast<const unsigned char*>(text.c_str()), text.size() + 1) < 0 || psa_crypto_init() != PSA_SUCCESS ||
                mbedtls_ctr_drbg_seed(&g_random, mbedtls_entropy_func, &g_entropy, reinterpret_cast<const unsigned char*>("nexgen"), 6) != 0 ||
                mbedtls_ssl_config_defaults(&g_config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0)
            {
                NX_ERROR("secure connections cannot be set up");
                return false;
            }
            mbedtls_ssl_conf_authmode(&g_config, MBEDTLS_SSL_VERIFY_REQUIRED);
            mbedtls_ssl_conf_verify(&g_config, checked, nullptr);
            mbedtls_ssl_conf_ca_chain(&g_config, &g_trusted, nullptr);
            mbedtls_ssl_conf_rng(&g_config, mbedtls_ctr_drbg_random, &g_random);
            g_failed = false;
            g_ready = true;
            return true;
        }
    }

    struct SecureStream::Parts
    {
        mbedtls_ssl_context ssl;
    };

    void secureSetPlatform(Platform* platform)
    {
        g_platform = platform;
    }

    void secureShutdown()
    {
        Lock lock(g_lock);
        if (g_ready)
        {
            mbedtls_ssl_config_free(&g_config);
            mbedtls_x509_crt_free(&g_trusted);
            mbedtls_ctr_drbg_free(&g_random);
            mbedtls_entropy_free(&g_entropy);
            mbedtls_psa_crypto_free();
            mbedtls_threading_free_alt();
        }
        g_ready = g_failed = false;
    }

    // A moment, unless there has been enough of that.
    bool SecureStream::waited()
    {
        if (m_waited > m_patience || m_cancelled->load())
        {
            m_error = m_cancelled->load() ? "called off" : "it took too long";
            return false;
        }
        sleepMilliseconds(4);
        m_waited += 0.004;
        return true;
    }

    int SecureStream::sendSome(void* self, const unsigned char* data, size_t size)
    {
        SecureStream& s = *static_cast<SecureStream*>(self);
        const size_t sent = s.m_socket->send(data, size);
        return sent ? static_cast<int>(sent) : s.m_socket->state() == TcpState::Open ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_SSL_CONN_EOF;
    }

    int SecureStream::receiveSome(void* self, unsigned char* data, size_t size)
    {
        SecureStream& s = *static_cast<SecureStream*>(self);
        const size_t got = s.m_socket->receive(data, size);
        return got ? static_cast<int>(got) : s.m_socket->state() == TcpState::Open ? MBEDTLS_ERR_SSL_WANT_READ : 0;
    }

    bool SecureStream::start(TcpSocket& socket, const char* host, const std::atomic<bool>& cancelled, double patience)
    {
        close();
        m_socket = &socket;
        m_cancelled = &cancelled;
        m_patience = patience;
        if (!prepare())
        {
            m_error = "there are no certificates to check it with";
            return false;
        }
        m_parts = create<Parts>(MemTag::General);
        if (m_parts == nullptr)
        {
            m_error = "no memory";
            return false;
        }
        mbedtls_ssl_init(&m_parts->ssl);
        if (mbedtls_ssl_setup(&m_parts->ssl, &g_config) != 0 || mbedtls_ssl_set_hostname(&m_parts->ssl, host) != 0)
        {
            m_error = "no memory";
            return false;
        }
        mbedtls_ssl_set_bio(&m_parts->ssl, this, sendSome, receiveSome, nullptr);
        for (;;)
        {
            const int result = mbedtls_ssl_handshake(&m_parts->ssl);
            if (result == 0)
            {
                return mbedtls_ssl_get_verify_result(&m_parts->ssl) == 0;
            }
            if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                m_error = result == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED ? "its certificate isn't to be trusted" : "it could not be spoken to securely";
                NX_WARN("%s: -0x%04x", m_error, static_cast<unsigned>(-result));
                return false;
            }
            if (!waited())
            {
                return false;
            }
        }
    }

    void SecureStream::close()
    {
        if (m_parts)
        {
            mbedtls_ssl_close_notify(&m_parts->ssl);
            mbedtls_ssl_free(&m_parts->ssl);
            destroy(m_parts);
            m_parts = nullptr;
        }
        m_waited = 0;
    }

    bool SecureStream::send(const void* data, size_t size)
    {
        size_t at = 0;
        while (at < size)
        {
            const int result = mbedtls_ssl_write(&m_parts->ssl, static_cast<const unsigned char*>(data) + at, size - at);
            if (result > 0)
            {
                at += static_cast<size_t>(result);
            }
            else if ((result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE) || !waited())
            {
                m_error = m_error[0] ? m_error : "it stopped listening";
                return false;
            }
        }
        return true;
    }

    size_t SecureStream::receiveNow(void* data, size_t size, bool& ended)
    {
        ended = false;
        for (;;)
        {
            const int result = mbedtls_ssl_read(&m_parts->ssl, static_cast<unsigned char*>(data), size);
            if (result > 0)
            {
                return static_cast<size_t>(result);
            }
            if (result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
            {
                continue;
            }
            ended = result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE;
            return 0;
        }
    }

    size_t SecureStream::receive(void* data, size_t size)
    {
        for (;;)
        {
            const int result = mbedtls_ssl_read(&m_parts->ssl, static_cast<unsigned char*>(data), size);
            if (result > 0)
            {
                m_waited = 0;
                return static_cast<size_t>(result);
            }
            // A ticket for next time, which isn't wanted, isn't the end either.
            if (result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
            {
                continue;
            }
            if ((result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE) || !waited())
            {
                return 0;
            }
        }
    }
}

// --- What Mbed TLS asks of the platform ---

extern "C"
{
    // Memory that held a secret. Not left out by the compiler, however little it thinks of it.
    void mbedtls_platform_zeroize(void* memory, size_t size)
    {
        volatile unsigned char* p = static_cast<volatile unsigned char*>(memory);
        while (p && size--)
        {
            *p++ = 0;
        }
    }

    int mbedtls_hardware_poll(void*, unsigned char* output, size_t size, size_t* got)
    {
        *got = 0;
#if defined(_XBOX)
        if (XNetRandom(output, static_cast<UINT>(size)) != 0)
        {
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }
#elif defined(_WIN32)
        if (BCryptGenRandom(nullptr, output, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        {
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }
#else
        FILE* file = fopen("/dev/urandom", "rb");
        const bool ok = file && fread(output, 1, size, file) == size;
        if (file)
        {
            fclose(file);
        }
        if (!ok)
        {
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }
#endif
        *got = size;
        return 0;
    }

    mbedtls_ms_time_t mbedtls_ms_time(void)
    {
#if defined(_XBOX) || defined(_WIN32)
        return static_cast<mbedtls_ms_time_t>(GetTickCount());
#else
        timeval t;
        gettimeofday(&t, nullptr);
        return static_cast<mbedtls_ms_time_t>(t.tv_sec) * 1000 + t.tv_usec / 1000;
#endif
    }

    // A date from seconds, by itself: not every platform's library has one that threads can share.
    struct tm* mbedtls_platform_gmtime_r(const mbedtls_time_t* seconds, struct tm* out)
    {
        memset(out, 0, sizeof(*out));
        long long days = static_cast<long long>(*seconds) / 86400;
        const long long left = static_cast<long long>(*seconds) % 86400;
        out->tm_hour = static_cast<int>(left / 3600);
        out->tm_min = static_cast<int>(left / 60 % 60);
        out->tm_sec = static_cast<int>(left % 60);
        // Years that begin in March, so that the odd day comes last.
        days += 719468;
        const long long era = days / 146097;
        const long long day = days - era * 146097;
        const long long year = (day - day / 1460 + day / 36524 - day / 146096) / 365;
        const long long dayOfYear = day - (365 * year + year / 4 - year / 100);
        const long long month = (5 * dayOfYear + 2) / 153;
        out->tm_mday = static_cast<int>(dayOfYear - (153 * month + 2) / 5 + 1);
        out->tm_mon = static_cast<int>(month < 10 ? month + 2 : month - 10);
        out->tm_year = static_cast<int>(year + era * 400 + (month >= 10 ? 1 : 0) - 1900);
        return out;
    }
}

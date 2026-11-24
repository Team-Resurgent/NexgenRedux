#include "base/log.h"
#include "base/memory.h"
#include "base/thread.h"

#include <pthread.h>
#include <time.h>

namespace nx
{
    namespace
    {
        struct Start
        {
            ThreadFunction function;
            void* user;
            pthread_t thread;
        };

        struct Counter
        {
            pthread_mutex_t mutex;
            pthread_cond_t changed;
            u32 count;
        };

        void* threadMain(void* data)
        {
            Start* start = static_cast<Start*>(data);
            start->function(start->user);
            return nullptr;
        }
    }

    Mutex::Mutex()
    {
        static_assert(sizeof(pthread_mutex_t) <= sizeof(m_storage));
        pthread_mutex_init(reinterpret_cast<pthread_mutex_t*>(m_storage), nullptr);
    }

    Mutex::~Mutex()
    {
        pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(m_storage));
    }

    void Mutex::lock()
    {
        pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(m_storage));
    }

    void Mutex::unlock()
    {
        pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(m_storage));
    }

    // Not sem_t: macOS has no unnamed semaphores.
    Semaphore::Semaphore()
    {
        static_assert(sizeof(Counter) <= sizeof(m_storage));
        Counter* c = reinterpret_cast<Counter*>(m_storage);
        pthread_mutex_init(&c->mutex, nullptr);
        pthread_cond_init(&c->changed, nullptr);
        c->count = 0;
    }

    Semaphore::~Semaphore()
    {
        Counter* c = reinterpret_cast<Counter*>(m_storage);
        pthread_cond_destroy(&c->changed);
        pthread_mutex_destroy(&c->mutex);
    }

    void Semaphore::post()
    {
        Counter* c = reinterpret_cast<Counter*>(m_storage);
        pthread_mutex_lock(&c->mutex);
        c->count++;
        pthread_cond_signal(&c->changed);
        pthread_mutex_unlock(&c->mutex);
    }

    void Semaphore::wait()
    {
        Counter* c = reinterpret_cast<Counter*>(m_storage);
        pthread_mutex_lock(&c->mutex);
        while (c->count == 0)
        {
            pthread_cond_wait(&c->changed, &c->mutex);
        }
        c->count--;
        pthread_mutex_unlock(&c->mutex);
    }

    ThreadHandle startThread(ThreadFunction function, void* user, ThreadPriority)
    {
        Start* start = create<Start>(MemTag::General);
        if (start == nullptr)
        {
            return {};
        }
        start->function = function;
        start->user = user;
        if (pthread_create(&start->thread, nullptr, threadMain, start) != 0)
        {
            NX_ERROR("cannot start a thread");
            destroy(start);
            return {};
        }
        return { start };
    }

    void joinThread(ThreadHandle thread)
    {
        Start* start = static_cast<Start*>(thread.value);
        pthread_join(start->thread, nullptr);
        destroy(start);
    }

    void sleepMilliseconds(u32 milliseconds)
    {
        timespec time = { static_cast<time_t>(milliseconds / 1000), static_cast<long>(milliseconds % 1000) * 1000000 };
        nanosleep(&time, nullptr);
    }
}

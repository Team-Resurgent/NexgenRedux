#pragma once

/* A lock, as Mbed TLS holds one: made and used in net/secure.cpp. */
typedef struct mbedtls_threading_mutex_t
{
    void* lock;
} mbedtls_threading_mutex_t;

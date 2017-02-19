#ifndef __LOCK_H__
#define __LOCK_H__

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
#include <windows.h>
typedef HANDLE mutex;
#else
#include <pthread.h>
typedef pthread_mutex_t mutex;
#endif

mutex * mutex_create();

void mutex_lock(mutex * m);

void mutex_unlock(mutex * m);

void mutex_release(mutex * m);

#endif

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)

mutex * mutex_create()
{
    mutex * m = malloc(sizeof(mutex));
    *m = CreateMutex(NULL, FALSE, NULL);
    return m;
}

void mutex_lock(mutex * m)
{
    WaitForSingleObject(*m, INFINITE);
}

void mutex_unlock(mutex * m)
{
    ReleaseMutex(*m);
}

void mutex_release(mutex * m)
{
    CloseHandle(*m);
    free(m);
}


#else



mutex * mutex_create()
{
    mutex * m = malloc(sizeof(mutex));
    pthread_mutex_init(m,NULL);
    return m;
}

void mutex_lock(mutex * m)
{
    pthread_mutex_lock(m);
}

void mutex_unlock(mutex * m)
{
    pthread_mutex_unlock(m);
}

void mutex_release(mutex * m)
{
    free(m);
}


#endif



#ifndef __THREADS_H__
#define __THREADS_H__


#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
#include <windows.h>
typedef HANDLE thread;
#else
#include <pthread.h>
typedef pthread_t thread;
#endif

typedef void (*runnable)(void);

thread * thread_create(runnable run);

void thread_join(thread * t);

void thread_release(thread * t);


#endif

struct func_holder
{
    runnable func;
};

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)

DWORD WINAPI _run(LPVOID target)
{
    struct func_holder * holder =  (struct func_holder*) target;
    holder->func();
    free(holder);
//ExitThread(0);
    return 1;
}

thread * thread_create(runnable run)
{
    thread * t = malloc(sizeof(thread));
    struct func_holder * holder = malloc(sizeof(struct func_holder));
    holder->func = run;
    *t =  CreateThread( NULL, 0, _run, holder, 0, NULL);
    return t;
}


void thread_join(thread * t)
{
    WaitForSingleObject(*t, INFINITE);
}

void thread_release(thread * t)
{
    CloseHandle(*t);
}


#else

void * _run(void* target)
{
    struct func_holder * holder =  (struct func_holder*) target;
    holder->func();
    free(holder);
    pthread_exit(NULL);
    return NULL;
}

thread * thread_create(runnable run)
{
    int ret = 0;
    thread * t = malloc(sizeof(thread));
    struct func_holder * holder = malloc(sizeof(struct func_holder));
    holder->func = run;
    ret = pthread_create( t, NULL, _run, holder);
    if (ret == 0)
    {
        return t;
    }
    return NULL;
}

void thread_join(thread * t)
{
    pthread_join(*t, NULL);
}

void thread_release(thread * t)
{
    free(t);
}

#endif

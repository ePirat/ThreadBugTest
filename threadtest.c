//
//  threadtest.c
//  This is a self-contained example to reproduce a
//  bug in Darwins pthread implementation.
//

// Force debug to be enabled
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/errno.h>

void vlc_cond_signal(pthread_cond_t *p_condvar)
{
    int val = pthread_cond_signal (p_condvar);
    assert(val == 0);
}

void vlc_cond_destroy(pthread_cond_t *p_condvar)
{
    int val = pthread_cond_destroy (p_condvar);
    assert(val == 0);
}

void vlc_mutex_destroy(pthread_mutex_t *p_mutex)
{
    int val = pthread_mutex_destroy(p_mutex);
    assert(val == 0);
}

void vlc_cond_init(pthread_cond_t *p_condvar)
{
    if (pthread_cond_init(p_condvar, NULL))
        abort();
}

void vlc_join(pthread_t handle, void **result)
{
    int val = pthread_join(handle, result);
    assert(val == 0);
}

void vlc_mutex_init(pthread_mutex_t *p_mutex)
{
    pthread_mutexattr_t attr;

    if (pthread_mutexattr_init(&attr))
        abort();

    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

    if (pthread_mutex_init(p_mutex, &attr))
        abort();
    pthread_mutexattr_destroy(&attr);
}

static inline void vlc_cleanup_lock(void *lock)
{
    int val = pthread_mutex_unlock((pthread_mutex_t *)lock);
    assert(val == 0);
}

void vlc_mutex_lock(pthread_mutex_t *p_mutex)
{
    int val = pthread_mutex_lock( p_mutex );
    assert(val == 0);
}

void vlc_cond_wait(pthread_cond_t *p_condvar, pthread_mutex_t *p_mutex)
{
    int val = pthread_cond_wait(p_condvar, p_mutex);
    assert(val == 0);
}

static int vlc_clone_attr(pthread_t *th, pthread_attr_t *attr,
                          void *(*entry) (void *), void *data, int priority)
{
    int ret;

#define VLC_STACKSIZE (128 * sizeof (void *) * 1024)

    ret = pthread_attr_setstacksize(attr, VLC_STACKSIZE);
    assert(ret == 0); /* fails iif VLC_STACKSIZE is invalid */

    ret = pthread_create(th, attr, entry, data);
    pthread_attr_destroy(attr);
    return ret;
}

int vlc_clone(pthread_t *th, void *(*entry) (void *), void *data,
              int priority)
{
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    return vlc_clone_attr(th, &attr, entry, data, priority);
}

// Thread data
typedef struct thread_data
{
    int num;
    pthread_cond_t cond;
    pthread_mutex_t lock;
} ThreadData;

// Init thread data (mutex and condition)
void thread_data_init(ThreadData *data, int tnum)
{
    data->num = tnum;
    vlc_cond_init(&data->cond);
    vlc_mutex_init(&data->lock);
}

// Clean up thread data (mutex and condition)
void thread_data_destory(ThreadData *data)
{
    vlc_cond_destroy(&data->cond);
    vlc_mutex_destroy(&data->lock);
}

// Function executed by the threads
void *thread_func(void *tdata)
{
    ThreadData *data = tdata;

    printf("Started thread %i\n", data->num);

    vlc_mutex_lock(&data->lock);
    pthread_cleanup_push(vlc_cleanup_lock, &data->lock);
    for (;;) {
        printf("Waiting on condition.\n");
        vlc_cond_wait(&data->cond, &data->lock);
        printf("Condition fulfilled or spurious wake.\n");
    }
    pthread_cleanup_pop(0);
    return NULL;
}

int main(int argc, char const *argv[])
{
    #define THREAD_COUNT 5

    // Threads
    pthread_t threads[THREAD_COUNT];
    ThreadData thread_data[THREAD_COUNT];

    // Start threads
    for (int i = 0; i < THREAD_COUNT; ++i) {
        printf("Starting thread %i...\n", i);

        thread_data_init(&thread_data[i], i);
        if (vlc_clone(&threads[i], thread_func, &thread_data[i], 0)) {
            printf("Failure starting thread %i!\n",i );
            return 1;
        }
    }

    sleep(1);

    // Teardown threads
    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        printf("Signaling condition for thread %i...\n", i);
        vlc_mutex_lock(&thread_data[i].lock);
        vlc_cond_signal(&thread_data[i].cond);
        printf("Signaled thread %i\n", i);

        int val = pthread_mutex_unlock(&thread_data[i].lock);
        assert(val == 0);

        printf("Cancelling thread %i...\n", i);
        pthread_cancel(threads[i]);

        vlc_join(threads[i], NULL);
        thread_data_destory(&thread_data[i]);

        printf("Terminated %i.\n", i);
    }

    return 0;
}

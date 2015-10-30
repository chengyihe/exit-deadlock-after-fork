#define LOG_TAG "ExitDeadlockAfterFork"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

bool thread_init = false;
pthread_mutex_t thread_init_mutex;
pthread_cond_t thread_init_cond;

class MyClass
{
public:
    MyClass()
    {
        pthread_mutex_init(&mutex, NULL);
        lockTime = 0;
        unlockTime = 0;
    }

    ~MyClass()
    {
        printf("%d:%d, %s starts\n", getpid(), gettid(), __func__);
        pthread_mutex_lock(&mutex);
        printf("%d:%d, lockTime = %d\n", getpid(), gettid(), lockTime);
        printf("%d:%d, unlockTime = %d\n", getpid(), gettid(), unlockTime);
        pthread_mutex_unlock(&mutex);
        printf("%d:%d, %s ends\n", getpid(), gettid(), __func__);
    }

    void lock()
    {
        pthread_mutex_lock(&mutex);
        lockTime++;
    }

    void unlock()
    {
        unlockTime++;
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_t mutex;
    int lockTime;
    int unlockTime;
};

MyClass myObj;

void *thread_routine(void *)
{
    pthread_mutex_lock(&thread_init_mutex);
    thread_init = true;
    pthread_cond_signal(&thread_init_cond);
    pthread_mutex_unlock(&thread_init_mutex);

    myObj.lock();
    printf("%d:%d, %s calls mObj.lock()\n", getpid(), gettid(), __func__);

    while (1) {
        usleep(1000000);
    }

    return NULL;
}

int main()
{
    pthread_t thread;
    int ret, status;
    pid_t pid;

    printf("%d:%d, exit-deadlock-after-fork starts\n", getpid(), gettid());
    pthread_mutex_init(&thread_init_mutex, NULL);
    pthread_cond_init(&thread_init_cond, NULL);

    ret = pthread_create(&thread, NULL, thread_routine, NULL);
    if (ret < 0) {
        printf("failed to pthread_create, error = %s\n", strerror(errno));
        return 1;
    }

    pthread_mutex_lock(&thread_init_mutex);
    while (!thread_init) {
        pthread_cond_wait(&thread_init_cond, &thread_init_mutex); } pthread_mutex_unlock(&thread_init_mutex);
    pthread_mutex_unlock(&thread_init_mutex);

    pid = fork();
    if (pid == 0) {
        printf("%d:%d, child process after fork\n", getpid(), gettid());
        printf("%d:%d, is ready to exit(1)\n", getpid(), gettid());
        exit(1);
    }

    printf("%d:%d, parent process after fork\n", getpid(), gettid());

    pid_t wpid  = waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        printf("%d:%d, child pid %d exited with status %d\n", getpid(), gettid(), wpid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("%d:%d, child pid %d killed by signal %d\n", getpid(), gettid(), wpid, WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        printf("%d:%d, child pid %d stopped by signal %d\n", getpid(), gettid(), wpid, WSTOPSIG(status));
    } else {
        printf("%d:%d, child pid %d state changed\n", getpid(), gettid(), wpid);
    }

    pthread_cond_destroy(&thread_init_cond);
    pthread_mutex_destroy(&thread_init_mutex);
    printf("%d:%d, exit-deadlock-after-fork ends\n", getpid(), gettid());

    return 0;
}

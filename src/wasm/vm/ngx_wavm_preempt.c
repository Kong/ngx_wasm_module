#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wavm_preempt.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/select.h>


typedef struct {
    sigjmp_buf      env;
    ngx_int_t       preempt_enabled;
    pthread_t       main_thread;
    int             efd_submit;
    int             efd_complete_request;
    int             efd_complete_ack;
    struct timeval  exec_timeout;
} ngx_wavm_preempt_state_t;


static void ngx_wavm_preempt_sighandler(int sig, siginfo_t *info, void *ucontext);
static void *ngx_wavm_preempt_monitor_func(void *data);


static ngx_wavm_preempt_state_t  preempt_state;


void
ngx_wavm_preempt_lock()
{
    sigset_t  mask, oldmask;

    if (preempt_state.preempt_enabled == 0) {
        return;
    }

    sigemptyset(&mask);
    sigaddset(&mask, NGX_WAVM_PREEMPT_SIGNAL);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    ngx_wasm_assert(sigismember(&oldmask, NGX_WAVM_PREEMPT_SIGNAL) == 0);
}


void
ngx_wavm_preempt_unlock()
{
    sigset_t   mask, oldmask;

    if (preempt_state.preempt_enabled == 0) {
        return;
    }

    sigemptyset(&mask);
    sigaddset(&mask, NGX_WAVM_PREEMPT_SIGNAL);
    sigprocmask(SIG_UNBLOCK, &mask, &oldmask);
    ngx_wasm_assert(sigismember(&oldmask, NGX_WAVM_PREEMPT_SIGNAL) == 1);
}


int
ngx_wavm_preempt_enter(ngx_wavm_preempt_enter_callback_pt callback, void *data)
{
    ngx_int_t  rc = 0;
    uint64_t   efd_buf = 1;

    if (preempt_state.preempt_enabled == 0) {
        callback(data);
        return NGX_OK;
    }

    if (sigsetjmp(preempt_state.env, 1)) {
        /* unwind */
        rc = NGX_ABORT;
        goto end;
    }

    /* notify the monitor thread */

    rc = write(preempt_state.efd_submit, &efd_buf, sizeof(efd_buf));
    if (rc < 0) {
        abort();
    }

    ngx_wavm_preempt_unlock();
    callback(data);

end:

    ngx_wavm_preempt_lock();

    rc = write(preempt_state.efd_complete_request, &efd_buf, sizeof(efd_buf));
    if (rc < 0) {
        abort();
    }

    rc = read(preempt_state.efd_complete_ack, &efd_buf, sizeof(efd_buf));
    if (rc < 0) {
        abort();
    }

    return rc;
}


ngx_int_t
ngx_wavm_preempt_init(struct timeval *exec_timeout)
{
    struct sigaction  sa;
    pthread_t         monitor_thread;

    preempt_state.preempt_enabled = 1;
    preempt_state.main_thread = pthread_self();
    preempt_state.efd_submit = eventfd(0, EFD_CLOEXEC|EFD_SEMAPHORE);
    preempt_state.efd_complete_request = eventfd(0, EFD_CLOEXEC|EFD_SEMAPHORE);
    preempt_state.efd_complete_ack = eventfd(0, EFD_CLOEXEC|EFD_SEMAPHORE);
    ngx_memcpy(&preempt_state.exec_timeout, exec_timeout,
               sizeof(struct timeval));

    ngx_wavm_preempt_lock();

    ngx_memzero(&sa, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = ngx_wavm_preempt_sighandler;

    sigaction(NGX_WAVM_PREEMPT_SIGNAL, &sa, NULL);

    if (preempt_state.efd_submit < 0
        || preempt_state.efd_complete_request < 0
        || preempt_state.efd_complete_ack < 0)
    {
        return NGX_ERROR;
    }

    /*
     * we use pthread_create here instead of Nginx's thread pool because this
     * is a single long-running thread and we already depend on pthreads anyway
     */
    if (pthread_create(&monitor_thread, NULL,
                       ngx_wavm_preempt_monitor_func, NULL) != 0)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_wavm_preempt_sighandler(int sig, siginfo_t *info, void *ucontext)
{
    ngx_int_t rc;

    if (ngx_wrt.interrupt_handler) {
        rc = ngx_wrt.interrupt_handler();

        if (rc == NGX_OK) {
            return;
        }
    }

    siglongjmp(preempt_state.env, 1);
}


static void *
ngx_wavm_preempt_monitor_func(void *data)
{
    /*
     * This function runs in its own thread and does not have access to
     * any Nginx API
     */

    int             rc;
    fd_set          rfds;
    struct timeval  tv;
    uint64_t        efd_buf;

    while (1) {
        /* wait for request */
        rc = read(preempt_state.efd_submit, &efd_buf, sizeof(efd_buf));
        if (rc < 0) {
            abort();
        }

        /* start ticking */
        ngx_memcpy(&tv, &preempt_state.exec_timeout, sizeof(struct timeval));
        FD_ZERO(&rfds);
        FD_SET(preempt_state.efd_complete_request, &rfds);
        rc = select(1, &rfds, NULL, NULL, &tv);

        if (rc == -1) {
            /* error */
            abort();
        }

        if (rc == 0) {
            /* timeout */
            pthread_kill(preempt_state.main_thread, NGX_WAVM_PREEMPT_SIGNAL);
        }

        rc = read(preempt_state.efd_complete_request, &efd_buf, sizeof(efd_buf));
        if (rc < 0) {
            abort();
        }

        rc = write(preempt_state.efd_complete_ack, &efd_buf, sizeof(efd_buf));
        if (rc < 0) {
            abort();
        }
    }
}

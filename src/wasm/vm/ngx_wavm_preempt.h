#ifndef _NGX_WAVM_PREEMPT_H_INCLUDED_
#define _NGX_WAVM_PREEMPT_H_INCLUDED_


#include <ngx_wasm.h>

/*
 * We follow Go's reasoning here:
 *
 * "We use SIGURG because it meets all of these criteria, is extremely
 *  unlikely to be used by an application for its "real" meaning (both
 *  because out-of-band data is basically unused and because SIGURG
 *  doesn't report which socket has the condition, making it pretty
 *  useless), and even if it is, the application has to be ready for
 *  spurious SIGURG. SIGIO wouldn't be a bad choice either, but is more
 *  likely to be used for real. "
 */
#define NGX_WAVM_PREEMPT_SIGNAL SIGURG


typedef void (*ngx_wavm_preempt_enter_callback_pt)(void *data);


void ngx_wavm_preempt_lock();
void ngx_wavm_preempt_unlock();
int ngx_wavm_preempt_enter(ngx_wavm_preempt_enter_callback_pt callback, void *data);
ngx_int_t ngx_wavm_preempt_init(struct timeval *exec_timeout);


#endif /* _NGX_WAVM_PREEMPT_H_INCLUDED_ */

#ifndef _NGX_WASI_H_INCLUDED_
#define _NGX_WASI_H_INCLUDED_


#include <ngx_wavm.h>


#define WASI_CLOCKID_REALTIME  0
#define WASI_CLOCKID_MONOTONIC 1

#define WASI_ERRNO_SUCCESS 0
#define WASI_ERRNO_NOTSUP  58


extern ngx_wavm_host_def_t  ngx_wasi_host;


#endif /* _NGX_WASI_H_INCLUDED_ */

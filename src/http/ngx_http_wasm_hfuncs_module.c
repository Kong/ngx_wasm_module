#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_wasm.h>
#include <ngx_wasm_vm_host.h>
#include <ngx_http.h>
#include <ngx_http_wasm_module.h>
#include <ngx_http_wasm_util.h>


ngx_int_t
ngx_http_wasm_hfuncs_resp_get_status(ngx_wasm_hctx_t *hctx,
   const wasm_val_t args[], wasm_val_t rets[])
{
   ngx_http_wasm_req_ctx_t  *rctx = hctx->data;
   ngx_http_request_t       *r = rctx->r;
   ngx_int_t                 status;

   if (r->connection->fd == (ngx_socket_t) -1) {
       return NGX_WASM_BAD_CTX;
   }

   if (r->err_status) {
       status = r->err_status;

   } else if (r->headers_out.status) {
       status = r->headers_out.status;

   } else if (r->http_version == NGX_HTTP_VERSION_9) {
       status = 9;

   } else {
       status = 0;
   }

   rets[0].kind = WASM_I32;
   rets[0].of.i32 = status;

   return NGX_WASM_OK;
}


ngx_int_t
ngx_http_wasm_hfuncs_resp_set_status(ngx_wasm_hctx_t *hctx,
   const wasm_val_t args[], wasm_val_t rets[])
{
   ngx_http_wasm_req_ctx_t  *rctx = hctx->data;
   ngx_http_request_t       *r = rctx->r;
   ngx_int_t                 status;

   if (r->connection->fd == (ngx_socket_t) -1) {
       return NGX_WASM_BAD_CTX;
   }

   if (r->header_sent) {
       ngx_wasm_hctx_trapmsg(hctx, "headers already sent");
       return NGX_WASM_BAD_USAGE;
   }

   status = args[0].of.i32;

   ngx_log_debug1(NGX_LOG_DEBUG_WASM, hctx->log, 0,
                  "wasm set resp status to %d", status);

   r->headers_out.status = status;

   if (r->err_status) {
       r->err_status = 0;
   }

   return NGX_WASM_OK;
}


ngx_int_t
ngx_http_wasm_hfuncs_resp_say(ngx_wasm_hctx_t *hctx,
   const wasm_val_t args[], wasm_val_t rets[])
{
   int64_t                   body_offset, len;
   ngx_int_t                 rc;
   ngx_buf_t                *b;
   ngx_chain_t              *cl;
   ngx_http_wasm_req_ctx_t  *rctx = hctx->data;
   ngx_http_request_t       *r = rctx->r;

   body_offset = args[0].of.i32;
   len = args[1].of.i32;

   if (r->connection->fd == (ngx_socket_t) -1) {
       return NGX_WASM_BAD_CTX;
   }

   b = ngx_create_temp_buf(r->pool, len + sizeof(LF));
   if (b == NULL) {
       return NGX_WASM_ERROR;
   }

   b->last = ngx_copy(b->last, hctx->mem_offset + body_offset, len);
   *b->last++ = LF;

   b->last_buf = 1;
   b->last_in_chain = 1;

   cl = ngx_alloc_chain_link(r->connection->pool);
   if (cl == NULL) {
       return NGX_WASM_ERROR;
   }

   cl->buf = b;
   cl->next = NULL;

   rc = ngx_http_wasm_send_chain_link(r, cl);
   if (rc == NGX_ERROR) {
       return NGX_WASM_ERROR;

   } else if (rc == NGX_AGAIN) {
       /* TODO: NYI - NGX_WASM_AGAIN */
       return NGX_AGAIN;
   }

   rctx->sent_last = 1;

   return NGX_WASM_OK;
}


static ngx_wasm_hdef_func_t  ngx_http_wasm_hfuncs[] = {

   { ngx_string("ngx_http_resp_get_status"),
     &ngx_http_wasm_hfuncs_resp_get_status,
     NULL,
     ngx_wasm_arity_i32 },

   { ngx_string("ngx_http_resp_set_status"),
     &ngx_http_wasm_hfuncs_resp_set_status,
     ngx_wasm_arity_i32,
     NULL },

   { ngx_string("ngx_http_resp_say"),
     &ngx_http_wasm_hfuncs_resp_say,
     ngx_wasm_arity_i32_i32,
     NULL },

   ngx_wasm_hfunc_null
};


static ngx_wasm_hdefs_t  ngx_http_wasm_hdefs = {
   NGX_WASM_HOST_SUBSYS_HTTP,
   ngx_http_wasm_hfuncs
};


static ngx_wasm_module_t  ngx_http_wasm_hfuncs_module_ctx = {
   NULL,                                /* runtime */
   &ngx_http_wasm_hdefs,                /* hdefs */
   NULL,                                /* create configuration */
   NULL,                                /* init configuration */
   NULL,                                /* init module */
};


ngx_module_t  ngx_http_wasm_hfuncs_module = {
   NGX_MODULE_V1,
   &ngx_http_wasm_hfuncs_module_ctx,    /* module context */
   NULL,                                /* module directives */
   NGX_WASM_MODULE,                     /* module type */
   NULL,                                /* init master */
   NULL,                                /* init module */
   NULL,                                /* init process */
   NULL,                                /* init thread */
   NULL,                                /* exit thread */
   NULL,                                /* exit process */
   NULL,                                /* exit master */
   NGX_MODULE_V1_PADDING
};

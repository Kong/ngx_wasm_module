#ifndef _NGX_WRT_H_INCLUDED_
#define _NGX_WRT_H_INCLUDED_


#include <ngx_core.h>


#define ngx_wrt_err_init(err)  ngx_memzero((err), sizeof(ngx_wrt_err_t))


typedef struct ngx_wavm_hfunc_s  ngx_wavm_hfunc_t;
typedef struct ngx_wavm_instance_s  ngx_wavm_instance_t;

typedef struct {
    ngx_str_t                      compiler;
} ngx_wavm_conf_t;


/* wasm runtime */


typedef enum {
    NGX_WRT_EXTERN_FUNC = 1,
    NGX_WRT_EXTERN_MEMORY,
} ngx_wrt_extern_kind_e;


#if NGX_WASM_HAVE_WASMTIME
#include <wasm.h>
#include <wasi.h>
#include <wasmtime.h>


typedef wasmtime_error_t  ngx_wrt_res_t;

typedef struct {
    wasm_engine_t                 *engine;
    wasmtime_linker_t             *linker;
} ngx_wrt_engine_t;

typedef struct {
    ngx_wrt_engine_t              *engine;
    wasmtime_module_t             *module;
    wasm_importtype_vec_t         *import_types;
    wasm_exporttype_vec_t         *export_types;
} ngx_wrt_module_t;

typedef struct {
    wasmtime_context_t            *context;
    wasmtime_store_t              *store;
} ngx_wrt_store_t;

typedef struct {
    ngx_pool_t                    *pool;
    ngx_wrt_store_t               *store;
    ngx_wrt_module_t              *module;
    wasi_config_t                 *wasi_config;
    wasmtime_memory_t             *memory;
    wasmtime_instance_t            instance;
} ngx_wrt_instance_t;

typedef struct {
    ngx_wrt_instance_t            *instance;
    wasmtime_context_t            *context;
    wasmtime_extern_t              ext;
    ngx_str_t                      name;
    ngx_wrt_extern_kind_e          kind;
} ngx_wrt_extern_t;

void ngx_wasm_valvec2wasmtime(wasmtime_val_t *out, wasm_val_vec_t *vec);
void ngx_wasmtime_valvec2wasm(wasm_val_vec_t *out, wasmtime_val_t *vec,
    size_t nvals);

#elif NGX_WASM_HAVE_WASMER
#include <wasmer.h>


typedef ngx_str_t  ngx_wrt_res_t;
typedef struct ngx_wrt_import_s  ngx_wrt_import_t;

typedef struct {
    ngx_pool_t                    *pool;
    wasm_config_t                 *wasm_config;
    wasi_config_t                 *wasi_config;
    wasi_env_t                    *wasi_env;
    wasm_engine_t                 *engine;
    wasm_store_t                  *store;
} ngx_wrt_engine_t;

typedef struct {
    ngx_wrt_engine_t              *engine;
    wasm_module_t                 *module;
    wasm_importtype_vec_t         *import_types;
    wasm_exporttype_vec_t         *export_types;
    ngx_wrt_import_t              *imports;
    ngx_uint_t                     nimports;
    unsigned                       wasi:1;
} ngx_wrt_module_t;

typedef struct {
    wasm_store_t                  *store;
    void                          *data;
} ngx_wrt_store_t;

typedef struct {
    ngx_wavm_hfunc_t              *hfunc;
    ngx_wavm_instance_t           *instance;
} ngx_wasmer_hfunc_ctx_t;

typedef struct {
    ngx_pool_t                    *pool;
    ngx_wrt_store_t               *store;
    ngx_wrt_module_t              *module;
    wasm_memory_t                 *memory;
    wasm_instance_t               *instance;
    ngx_wasmer_hfunc_ctx_t        *ctxs;
    wasmer_named_extern_vec_t      wasi_imports;
    wasm_extern_vec_t              env;
    wasm_extern_vec_t              externs;
} ngx_wrt_instance_t;

typedef struct {
    ngx_wrt_instance_t            *instance;
    wasm_name_t                   *name;
    wasm_extern_t                 *ext;
    ngx_wrt_extern_kind_e          kind;
} ngx_wrt_extern_t;

#elif NGX_WASM_HAVE_V8
#include <wasm.h>


/* The copy of wasm.h that ships with V8 does not include these definitions: */

#ifndef WASM_I32_VAL
#define WASM_I32_VAL(i) {.kind = WASM_I32, .of = {.i32 = i}}
#endif

#ifndef WASM_I64_VAL
#define WASM_I64_VAL(i) {.kind = WASM_I64, .of = {.i64 = i}}
#endif

#ifndef WASM_F32_VAL
#define WASM_F32_VAL(z) {.kind = WASM_F32, .of = {.f32 = z}}
#endif

#ifndef WASM_F64_VAL
#define WASM_F64_VAL(z) {.kind = WASM_F64, .of = {.f64 = z}}
#endif

#ifndef WASM_REF_VAL
#define WASM_REF_VAL(r) {.kind = WASM_ANYREF, .of = {.ref = r}}
#endif

#ifndef WASM_INIT_VAL
#define WASM_INIT_VAL {.kind = WASM_ANYREF, .of = {.ref = NULL}}
#endif


typedef wasm_byte_vec_t  ngx_wrt_res_t;

typedef enum {
    NGX_WRT_IMPORT_HFUNC,
    NGX_WRT_IMPORT_WASI,
} ngx_wrt_import_kind_e;

typedef struct {
    ngx_pool_t                    *pool;
    wasm_engine_t                 *engine;
    wasm_store_t                  *store;
    unsigned                       exiting:1;
} ngx_wrt_engine_t;

typedef struct {
    ngx_wrt_engine_t              *engine;
    wasm_module_t                 *module;
    wasm_importtype_vec_t         *import_types;
    wasm_exporttype_vec_t         *export_types;
    ngx_wavm_hfunc_t             **imports;
    ngx_wrt_import_kind_e         *import_kinds;
    ngx_uint_t                     nimports;
} ngx_wrt_module_t;

typedef struct {
    wasm_store_t                  *store;
    void                          *data;
} ngx_wrt_store_t;

typedef struct {
    ngx_wavm_hfunc_t              *hfunc;
    ngx_wavm_instance_t           *instance;
} ngx_v8_hfunc_ctx_t;

typedef struct {
    ngx_pool_t                    *pool;
    ngx_wrt_store_t               *store;
    ngx_wrt_module_t              *module;
    wasm_memory_t                 *memory;
    wasm_instance_t               *instance;
    ngx_v8_hfunc_ctx_t            *ctxs;
    wasm_extern_t                **imports;
    wasm_extern_vec_t              externs;
} ngx_wrt_instance_t;

typedef struct {
    ngx_wrt_instance_t            *instance;
    wasm_name_t                   *name;
    wasm_extern_t                 *ext;
    ngx_wrt_extern_kind_e          kind;
} ngx_wrt_extern_t;
#endif  /* NGX_WASM_HAVE_* */


typedef struct {
    wasm_trap_t     *trap;
    ngx_wrt_res_t   *res;
} ngx_wrt_err_t;


typedef struct {
    ngx_int_t                    (*conf_init)(wasm_config_t *config,
                                              ngx_wavm_conf_t *conf,
                                              ngx_log_t *log);
    ngx_int_t                    (*engine_init)(ngx_wrt_engine_t *engine,
                                                wasm_config_t *config,
                                                ngx_pool_t *pool,
                                                ngx_wrt_err_t *err);
    void                         (*engine_destroy)(ngx_wrt_engine_t *engine);
    ngx_int_t                    (*validate)(ngx_wrt_engine_t *engine,
                                             wasm_byte_vec_t *bytes,
                                             ngx_wrt_err_t *err);
    ngx_int_t                    (*wat2wasm)(wasm_byte_vec_t *wat,
                                             wasm_byte_vec_t *wasm,
                                             ngx_wrt_err_t *err);
    ngx_int_t                    (*module_init)(ngx_wrt_module_t *module,
                                                ngx_wrt_engine_t *engine,
                                                wasm_byte_vec_t *bytes,
                                                wasm_importtype_vec_t *imports,
                                                wasm_exporttype_vec_t *exports,
                                                ngx_wrt_err_t *err);
    ngx_int_t                    (*module_link)(ngx_wrt_module_t *module,
                                                ngx_array_t *hfuncs,
                                                ngx_wrt_err_t *err);
    void                         (*module_destroy)(ngx_wrt_module_t *module);
    ngx_int_t                    (*store_init)(ngx_wrt_store_t *store,
                                               ngx_wrt_engine_t *engine,
                                               void *data);
    void                         (*store_destroy)(ngx_wrt_store_t *store);
    ngx_int_t                    (*instance_init)(ngx_wrt_instance_t *instance,
                                                  ngx_wrt_store_t *store,
                                                  ngx_wrt_module_t *module,
                                                  ngx_pool_t *pool,
                                                  ngx_wrt_err_t *err);
    void                         (*instance_destroy)(
                                         ngx_wrt_instance_t *instance);
    ngx_int_t                    (*call)(ngx_wrt_instance_t *instance,
                                         ngx_str_t *func_name,
                                         ngx_uint_t func_idx,
                                         wasm_val_vec_t *args,
                                         wasm_val_vec_t *rets,
                                         ngx_wrt_err_t *err);
    ngx_int_t                    (*extern_init)(ngx_wrt_extern_t *ext,
                                                ngx_wrt_instance_t *instance,
                                                ngx_uint_t idx);
    void                         (*extern_destroy)(ngx_wrt_extern_t *ext);
    wasm_trap_t                 *(*trap_new)(ngx_wrt_store_t *store,
                                             wasm_byte_vec_t *msg);
    void                        *(*get_ctx)(void *data);
    u_char                      *(*log_handler)(ngx_wrt_res_t *res,
                                                u_char *buf, size_t len);
} ngx_wrt_t;


extern ngx_wrt_t  ngx_wrt;


void ngx_wavm_log_error(ngx_uint_t level, ngx_log_t *log, ngx_wrt_err_t *e,
    const char *fmt, ...);


#endif /* _NGX_WRT_H_INCLUDED_ */

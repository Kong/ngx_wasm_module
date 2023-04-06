#ifndef _NGX_WASM_BACKTRACE_H_INCLUDED_
#define _NGX_WASM_BACKTRACE_H_INCLUDED_


#include <wasm.h>
#include <stdint.h>
#include <stdbool.h>


typedef struct {
    uint32_t                    idx;
    wasm_byte_vec_t            *name;
} ngx_wasm_backtrace_name_t;


typedef struct {
    uint64_t                    size;
    ngx_wasm_backtrace_name_t  *table;
} ngx_wasm_backtrace_name_table_t;


ngx_wasm_backtrace_name_table_t* ngx_wasm_backtrace_get_name_table(
    const wasm_byte_vec_t *wasm);
void ngx_wasm_backtrace_free_name_table(ngx_wasm_backtrace_name_table_t* t);
void ngx_wasm_backtrace_demangle(const wasm_byte_vec_t *mangled,
    wasm_byte_vec_t *demangled);
void ngx_wasm_backtrace_drop_byte_vec(const wasm_byte_vec_t *vec);


#endif /* _NGX_WASM_BACKTRACE_H_INCLUDED_ */

-- vim:set ts=4 sw=4 sts=4 et:

local ffi = require "ffi"
local base = require "resty.core.base"


local C = ffi.C
local assert = assert
local get_string_buf = base.get_string_buf
local get_size_ptr = base.get_size_ptr
local DEBUG_MODE = ngx.config.debug


ffi.cdef [[
    typedef struct {
        ngx_str_t       *name;
        ngx_str_t       *config;
    } ngx_wasm_filter_t;

    typedef unsigned char                 u_char;
    typedef intptr_t                      ngx_int_t;
    typedef uintptr_t                     ngx_uint_t;
    typedef ngx_uint_t                    ngx_msec_t;
    typedef struct ngx_log_s              ngx_log_t;
    typedef struct ngx_wavm_t             ngx_wasm_vm_t;
    typedef struct ngx_wasm_ops_plan_t    ngx_wasm_plan_t;

    typedef ngx_int_t (*ngx_proxy_wasm_properties_ffi_handler_pt)(void *data,
        ngx_str_t *key, ngx_str_t *value, ngx_str_t *err);

    ngx_wasm_vm_t *ngx_wasm_ffi_main_vm();
]]


local ERR_BUF_SIZE = 256


local _M = {
    FFI_DECLINED = base.FFI_DECLINED,
    FFI_ABORT = base.FFI_ABORT  -- nil in OpenResty 1.21.4.1
                and base.FFI_ABORT
                or -6,
    FFI_BUSY = base.FFI_BUSY  -- nil in OpenResty 1.25.3.2
               and base.FFI_BUSY
               or -3,
    FFI_ERROR = base.FFI_ERROR,
    FFI_DONE = base.FFI_DONE,
    FFI_OK = base.FFI_OK,

    get_request = base.get_request,
}


function _M.get_main_vm()
    return C.ngx_wasm_ffi_main_vm()
end


function _M.get_err_ptr()
    local errbuf = get_string_buf(ERR_BUF_SIZE)
    local errlen = get_size_ptr()

    errlen[0] = 0

    return errbuf, errlen
end


function _M.assert_debug(...)
    if DEBUG_MODE then
        return assert(...)
    end
end


return _M

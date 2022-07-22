-- vim:set st=4 sw=4 sts=4 et:

local ffi = require "ffi"


local C = ffi.C
local ffi_new = ffi.new
local ffi_str = ffi.string
local find    = string.find
local error   = error
local type    = type
local subsystem = ngx.config.subsystem


ffi.cdef [[
    struct ngx_wasm_ffi_filter_t {
        ngx_str_t       *name;
        ngx_str_t       *config;
        ngx_uint_t       filter_id;
    };

    typedef struct ngx_wavm_t             ngx_wasm_vm_t;
    typedef struct ngx_wasm_ops_t         ngx_wasm_ops_t;
    typedef struct ngx_wasm_ffi_filter_t  ngx_wasm_filter_t;

    ngx_wasm_vm_t *ngx_wasm_ffi_main_vm();
]]


local _M = {}


function _M.get_main_vm()
    return C.ngx_wasm_ffi_main_vm()
end


return _M

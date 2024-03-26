extern "C" {
    fn ngx_wasm_lua_test_argsrets();
    fn ngx_wasm_lua_test_bad_chunk();
    fn ngx_wasm_lua_test_error();
    fn ngx_wasm_lua_test_sleep();
}

#[no_mangle]
pub fn test_lua_argsrets() {
    unsafe { ngx_wasm_lua_test_argsrets() }
}

#[no_mangle]
pub fn test_bad_lua_chunk() {
    unsafe { ngx_wasm_lua_test_bad_chunk() }
}

#[no_mangle]
pub fn test_lua_error() {
    unsafe { ngx_wasm_lua_test_error() }
}

#[no_mangle]
pub fn test_lua_sleep() {
    unsafe { ngx_wasm_lua_test_sleep() }
}

extern "C" {
    fn ngx_wasm_lua_test_argsrets();
    fn ngx_wasm_lua_test_bad_chunk();
}

#[no_mangle]
pub fn test_lua_argsrets() {
    unsafe { ngx_wasm_lua_test_argsrets() }
}

#[no_mangle]
pub fn test_bad_lua_chunk() {
    unsafe { ngx_wasm_lua_test_bad_chunk() }
}

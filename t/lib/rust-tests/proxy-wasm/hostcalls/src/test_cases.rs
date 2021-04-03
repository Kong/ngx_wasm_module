use crate::*;
use log::*;
use proxy_wasm::hostcalls::*;
use proxy_wasm::types::*;

pub(crate) fn test_log(path: &str, _: &TestHttpHostcalls) {
    match path {
        "levels" => {
            trace!("proxy_log trace");
            info!("proxy_log info");
            warn!("proxy_log warn");
            error!("proxy_log error");
            log(LogLevel::Critical, "proxy_log critical").unwrap();
        }
        _ => {}
    }
}

pub(crate) fn test_send_http_response(path: &str, ctx: &TestHttpHostcalls) {
    match path {
        "status/204" => ctx.send_http_response(204, vec![], None),
        "status/1000" => ctx.send_http_response(1000, vec![], None),
        "headers" => ctx.send_http_response(200, vec![("Powered-By", "proxy-wasm")], None),
        "body" => ctx.send_http_response(200, vec![], Some("Hello world".as_bytes())),
        _ => {}
    }
}

pub(crate) fn test_log_http_request_headers(_: &str, ctx: &TestHttpHostcalls)
where
    TestHttpHostcalls: HttpContext,
{
    for (name, value) in &ctx.get_http_request_headers() {
        info!("#{} -> {}: {}", ctx.context_id, name, value);
    }
}

pub(crate) fn test_get_http_request_header(path: &str, ctx: &TestHttpHostcalls)
where
    TestHttpHostcalls: HttpContext,
{
    let value = ctx.get_http_request_header(path).unwrap();
    let body = format!("{}: {}", path, value);
    send_http_response(200, vec![], Some(body.as_bytes())).unwrap();
}

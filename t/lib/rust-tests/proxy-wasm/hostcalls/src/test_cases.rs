use crate::*;
use log::*;
use proxy_wasm::hostcalls::*;
use proxy_wasm::types::*;

pub(crate) fn test_log(path: &str, _: &HttpHeaders) {
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

pub(crate) fn test_send_http_response(path: &str, _: &HttpHeaders) {
    match path {
        "status" => send_http_response(204, vec![], None).unwrap(),
        _ => {}
    }
}

pub(crate) fn test_get_http_request_headers(_: &str, ctx: &HttpHeaders)
where
    HttpHeaders: HttpContext,
{
    for (name, value) in &ctx.get_http_request_headers() {
        info!("#{} -> {}: {}", ctx.context_id, name, value);
    }
}

pub(crate) fn test_get_http_request_header(path: &str, ctx: &HttpHeaders)
where
    HttpHeaders: HttpContext,
{
    let value = ctx.get_http_request_header(path).unwrap();
    let body = format!("{}: {}", path, value);
    send_http_response(200, vec![], Some(body.as_bytes())).unwrap();
}

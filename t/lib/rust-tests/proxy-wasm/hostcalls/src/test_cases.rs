use crate::*;
use chrono::{DateTime, Utc};
use log::*;
use proxy_wasm::hostcalls::*;
use proxy_wasm::types::*;

pub(crate) fn test_log_levels(_: &mut TestHttpHostcalls) {
    trace!("proxy_log trace");
    info!("proxy_log info");
    warn!("proxy_log warn");
    error!("proxy_log error");
    log(LogLevel::Critical, "proxy_log critical").unwrap();
}

pub(crate) fn test_log_current_time(ctx: &mut TestHttpHostcalls) {
    let now: DateTime<Utc> = ctx.get_current_time().into();
    info!("now: {}", now)
}

pub(crate) fn test_send_status(ctx: &mut TestHttpHostcalls, status: u32) {
    ctx.send_http_response(status, vec![], None)
}

pub(crate) fn test_send_headers(ctx: &mut TestHttpHostcalls) {
    ctx.send_http_response(200, vec![("Powered-By", "proxy-wasm")], None)
}

pub(crate) fn test_send_body(ctx: &mut TestHttpHostcalls) {
    ctx.send_http_response(
        200,
        vec![("Content-Length", "0")],
        Some("Hello world".as_bytes()),
    )
}

pub(crate) fn test_set_special_headers(ctx: &mut TestHttpHostcalls) {
    ctx.send_http_response(
        200,
        vec![
            ("Server", "proxy-wasm"),
            ("Date", "Wed, 22 Oct 2020 07:28:00 GMT"),
            ("Content-Length", "0"),
            ("Content-Encoding", "gzip"),
            ("Location", "/index.html"),
            ("Refresh", "5; url=http://www.w3.org/index.html"),
            ("Last-Modified", "Tue, 15 Nov 1994 12:45:26 GMT"),
            ("Content-Range", "bytes 21010-47021/47022"),
            ("Accept-Ranges", "bytes"),
            ("WWW-Authenticate", "Basic"),
            ("Expires", "Thu, 01 Dec 1994 16:00:00 GMT"),
            //("ETag", "737060cd8c284d8af7ad3082f209582d"), // TODO
            ("E-Tag", "377060cd8c284d8af7ad3082f20958d2"),
            ("Content-Type", "text/plain; charset=UTF-8"),
            ("Cache-Control", "no-cache"),
            ("Link", "</feed>; rel=\"alternate\""),
        ],
        None,
    )
}

pub(crate) fn test_set_headers_escaping(ctx: &mut TestHttpHostcalls) {
    ctx.send_http_response(
        200,
        vec![
            ("Escape-Colon:", "value"),
            ("Escape-Parenthesis()", "value"),
            ("Escape-Quote\"", "value"),
            ("Escape-Comps<>", "value"),
            ("Escape-Equal=", "value"),
        ],
        None,
    )
}

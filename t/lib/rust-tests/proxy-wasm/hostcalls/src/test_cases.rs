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
        "body" => ctx.send_http_response(
            200,
            vec![("Content-Length", "0")],
            Some("Hello world".as_bytes()),
        ),
        "special_headers" => ctx.send_http_response(
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
                ("Content-Type", "text/plain"),
                ("Cache-Control", "no-cache"),
                ("Link", "</feed>; rel=\"alternate\""),
            ],
            None,
        ),
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

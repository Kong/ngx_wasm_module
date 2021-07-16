use crate::*;
use http::StatusCode;

pub(crate) fn echo_headers(ctx: &mut TestHttpHostcalls) {
    let headers = ctx
        .get_http_request_headers()
        .iter()
        .filter_map(|(name, value)| {
            // Do not include ':path' and such special headers
            // Do not include 'pwm-*' test headers
            if name.to_lowercase().starts_with("pwm-") || name.to_lowercase().starts_with(':') {
                return None;
            }
            Some((name, value))
        })
        .map(|(name, value)| format!("{}: {}", name, value))
        .collect::<Vec<String>>()
        .join("\n");

    ctx.send_plain_response(StatusCode::OK, Some(headers));
}

pub(crate) fn echo_all_headers(ctx: &mut TestHttpHostcalls) {
    let headers = ctx
        .get_http_request_headers()
        .iter()
        .map(|(name, value)| format!("{}: {}", name, value))
        .collect::<Vec<String>>()
        .join("\n");

    ctx.send_plain_response(StatusCode::OK, Some(headers));
}

pub(crate) fn echo_header(ctx: &mut TestHttpHostcalls, name: &str) {
    let _ = ctx.get_http_request_header(name);
    let value = ctx.get_http_request_header(name); // twice for host cache hit
    if value.is_none() {
        ctx.send_plain_response(StatusCode::OK, Some(format!("{}:", name)));
        return;
    }

    let body = format!("{}: {}", name, value.unwrap());
    ctx.send_plain_response(StatusCode::OK, Some(body));
}

pub(crate) fn echo_body(ctx: &mut TestHttpHostcalls, max_size: Option<usize>) {
    let max = max_size.unwrap_or(usize::MAX);
    let body = ctx
        .get_http_request_body(0, max)
        .map(|bytes| String::from_utf8(bytes).expect("Invalid UTF-8 sequence"));

    info!("/echo/body buffer: {:?}", body);

    ctx.send_plain_response(StatusCode::OK, body)
}

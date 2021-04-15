use crate::*;
use http::StatusCode;

pub(crate) fn echo_headers(ctx: &mut TestHttpHostcalls) {
    let headers = ctx
        .get_http_request_headers()
        .iter()
        .map(|(name, value)| format!("{}: {}", name, value))
        .collect::<Vec<String>>()
        .join("\r\n");

    ctx.send_plain_response(StatusCode::OK, Some(headers));
}

pub(crate) fn echo_header(ctx: &mut TestHttpHostcalls, name: &str) {
    let value = ctx.get_http_request_header(name);
    if value.is_none() {
        ctx.send_plain_response(StatusCode::OK, Some(format!("{}:", name)));
        return;
    }

    let body = format!("{}: {}", name, value.unwrap());
    ctx.send_plain_response(StatusCode::OK, Some(body));
}

use crate::*;
use http::StatusCode;

pub(crate) fn echo_headers(ctx: &mut TestHttpHostcalls) {
    let headers = ctx
        .get_http_request_headers()
        .iter()
        .filter_map(|(name, value)| {
            // Do not include pwm-* test headers
            if name.to_lowercase().starts_with("pwm-") {
                return None;
            }
            Some((name, value))
        })
        .map(|(name, value)| format!("{}: {}", name, value))
        .collect::<Vec<String>>()
        .join("\n");

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

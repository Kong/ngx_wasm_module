use crate::*;
use http::StatusCode;
use std::str;

pub(crate) fn echo_headers(ctx: &mut TestHttp) {
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
        .map(|(name, value)| format!("{name}: {value}"))
        .collect::<Vec<String>>()
        .join("\n");

    ctx.send_plain_response(StatusCode::OK, Some(headers.as_str()));
}

pub(crate) fn echo_all_headers(ctx: &mut TestHttp) {
    let headers = ctx
        .get_http_request_headers()
        .iter()
        .map(|(name, value)| format!("{name}: {value}"))
        .collect::<Vec<String>>()
        .join("\n");

    ctx.send_plain_response(StatusCode::OK, Some(headers.as_str()));
}

pub(crate) fn echo_header(ctx: &mut TestHttp, name: &str) {
    let _ = ctx.get_http_request_header(name);
    let value = ctx.get_http_request_header(name); // twice for host cache hit
    if value.is_none() {
        ctx.send_plain_response(StatusCode::OK, Some(format!("{name}:").as_str()));
        return;
    }

    let body = format!("{name}: {}", value.unwrap());
    ctx.send_plain_response(StatusCode::OK, Some(body.as_str()));
}

pub(crate) fn echo_body(ctx: &mut TestHttp) {
    let req_body = ctx.get_http_request_body(0, usize::MAX);
    info!("/echo/body buffer: {:?}", req_body);

    if let Some(body) = req_body {
        ctx.send_plain_response(
            StatusCode::OK,
            Some(str::from_utf8(&body).expect("Invalid UTF-8 sequence")),
        );
    } else {
        ctx.send_plain_response(StatusCode::OK, None);
    }
}

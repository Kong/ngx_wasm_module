use crate::*;
use parse_duration::parse;
use std::time::Duration;

pub(crate) fn dispatch_call(ctx: &mut TestHttpHostcalls) {
    let mut timeout = Duration::from_secs(0);
    let mut headers = Vec::new();

    if ctx.config.get("no_method").is_none() {
        headers.push((
            ":method",
            ctx.config
                .get("method")
                .map(|v| v.as_str())
                .unwrap_or("GET"),
        ));
    }

    if ctx.config.get("no_path").is_none() {
        headers.push((
            ":path",
            ctx.config.get("path").map(|v| v.as_str()).unwrap_or("/"),
        ));
    }

    if ctx.config.get("use_https").map(|x| x.as_str() == "yes") == Some(true) {
        headers.push((
            ":scheme",
            "https",
        ));
    }

    if ctx.config.get("no_authority").is_none() {
        headers.push((
            ":authority",
            ctx.config
                .get("host")
                .map(|v| v.as_str())
                .unwrap_or("default"),
        ));
    }

    if let Some(vals) = ctx.config.get("headers") {
        for (k, v) in vals.split('|').filter_map(|s| s.split_once(':')) {
            headers.push((k, v));
        }
    }

    if let Some(val) = ctx.config.get("timeout") {
        if let Ok(t) = parse(val) {
            timeout = t
        }
    }

    ctx.dispatch_http_call(
        ctx.config.get("host").map(|v| v.as_str()).unwrap_or(""),
        headers,
        ctx.config.get("body").map(|v| v.as_bytes()),
        vec![],
        timeout,
    )
    .unwrap();
}

use crate::*;
use std::time::Duration;

pub(crate) fn dispatch_call(ctx: &mut TestHttpHostcalls) {
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

    if ctx.config.get("no_authority").is_none() {
        headers.push((
            ":authority",
            ctx.config
                .get("host")
                .map(|v| v.as_str())
                .unwrap_or("default"),
        ));
    }

    ctx.dispatch_http_call(
        ctx.config.get("host").map(|v| v.as_str()).unwrap_or(""),
        headers,
        None,
        vec![],
        Duration::from_secs(5),
    )
    .unwrap();
}

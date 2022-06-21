use http::StatusCode;
use log::*;
use parse_duration::parse;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::collections::HashMap;
use std::time::Duration;

struct TestRoot {
    config: HashMap<String, String>,
}

impl Context for TestRoot {}
impl RootContext for TestRoot {
    fn on_configure(&mut self, _: usize) -> bool {
        if let Some(config_bytes) = self.get_configuration() {
            let config_str = String::from_utf8(config_bytes).unwrap();
            self.config = config_str
                .split_whitespace()
                .filter_map(|s| s.split_once('='))
                .map(|(k, v)| (k.to_string(), v.to_string()))
                .collect();
        } else {
            self.config = HashMap::new();
        }

        true
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, _: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(TestHttpHostcalls {
            config: self.config.clone(),
        }))
    }
}

struct TestHttpHostcalls {
    config: HashMap<String, String>,
}

impl Context for TestHttpHostcalls {
    fn on_http_call_response(
        &mut self,
        _token_id: u32,
        _num_headers: usize,
        _body_size: usize,
        _num_trailers: usize,
    ) {
        if let Some(bytes) = self.get_http_call_response_body(0, 1000) {
            let body = String::from_utf8(bytes).unwrap();
            let response = body.trim().to_string();
            info!("[hostcalls] http_call_response: {:?}", response);

            match self
                .config
                .get("echo_response")
                .unwrap_or(&String::new())
                .as_str()
            {
                "off" | "false" | "F" => {}
                _ => self.send_http_response(
                    StatusCode::OK.as_u16() as u32,
                    vec![],
                    Some(response.as_bytes()),
                ),
            }
        }

        self.resume_http_request()
    }
}

impl HttpContext for TestHttpHostcalls {
    fn on_http_request_headers(&mut self, _: usize) -> Action {
        if self.config.get("host").is_some() {
            dispatch(self);
            return Action::Pause;
        } else if let Some(test) = self.config.get("test") {
            match test.as_str() {
                "proxy_get_header_map_value" => {
                    let ct = self.get_http_request_header("Connection").unwrap();
                    self.send_http_response(
                        StatusCode::OK.as_u16() as u32,
                        vec![],
                        Some(ct.as_bytes()),
                    );
                }
                _ => todo!(),
            };
        }

        Action::Continue
    }
}

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(TestRoot {
            config: HashMap::new(),
        })
    });
}

fn dispatch(ctx: &mut TestHttpHostcalls) {
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

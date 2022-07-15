use log::info;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;

struct HttpHeadersRoot {
}

impl Context for HttpHeadersRoot {}
impl RootContext for HttpHeadersRoot {
    fn on_vm_start(&mut self, _config_size: usize) -> bool {
        true
    }

    fn on_configure(&mut self, _config_size: usize) -> bool {
        true
    }

    fn on_tick(&mut self) {
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(OnPhases {
            context_id,
        }))
    }
}

struct OnPhases {
    context_id: u32,
}

impl Context for OnPhases {}
impl HttpContext for OnPhases {
    fn on_http_request_headers(&mut self, _nheaders: usize, _eof: bool) -> Action {
        let path = self.get_http_request_header(":path").unwrap();
        info!("#{} on_http_request_headers {}", self.context_id, path);
        match path.as_str() {
            "/t/hash" => {
                let prefix = self.get_http_request_header("x-prefix").expect("missing x-prefix");
                for i in 0u64.. {
                    let s = format!("{}-{}", prefix, i);
                    let hash = blake3::hash(s.as_bytes());
                    let bytes = hash.as_bytes();
                    if bytes[0] == 0 && bytes[1] == 0 {
                        self.send_http_response(200, vec![], Some(s.as_bytes()));
                        break;
                    }
                }
            }
            _ => {}
        }
        Action::Continue
    }

    fn on_http_request_body(&mut self, _len: usize, _end_of_stream: bool) -> Action {
        Action::Continue
    }

    fn on_http_response_headers(&mut self, _nheaders: usize, _eof: bool) -> Action {
        Action::Continue
    }

    fn on_http_response_body(&mut self, _len: usize, _end_of_stream: bool) -> Action {
        Action::Continue
    }

    fn on_log(&mut self) {
        info!("#{} on_log", self.context_id);
    }
}

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Warn);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(HttpHeadersRoot {
        })
    });
}

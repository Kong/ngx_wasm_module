mod fannkuch_redux;

use log::info;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;

struct BenchmarksRoot {}

impl Context for BenchmarksRoot {}
impl RootContext for BenchmarksRoot {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, _context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(Benchmarks {}))
    }
}

struct Benchmarks {}

impl Context for Benchmarks {}
impl HttpContext for Benchmarks {
    fn on_http_request_headers(&mut self, _nheaders: usize, _eof: bool) -> Action {
        let path = self.get_http_request_header(":path").unwrap();
        info!("on_http_request_headers {}", path);
        match path.as_str() {
            "/t/hash" => {
                let prefix = self
                    .get_http_request_header("x-prefix")
                    .expect("missing x-prefix");
                for i in 0u64.. {
                    let s = format!("{prefix}-{i}");
                    let hash = blake3::hash(s.as_bytes());
                    let bytes = hash.as_bytes();
                    if bytes[0] == 0 && bytes[1] == 0 {
                        self.send_http_response(200, vec![], Some(s.as_bytes()));
                        break;
                    }
                }
            }
            "/t/fannkuch_redux" => {
                let n = self
                    .get_http_request_header("x-n")
                    .expect("missing x-n")
                    .parse::<usize>()
                    .unwrap();
                let output = fannkuch_redux::run(n);
                self.send_http_response(200, vec![], Some(output.as_bytes()));
            }
            _ => {}
        }
        Action::Continue
    }
}

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Warn);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> { Box::new(BenchmarksRoot {}) });
}

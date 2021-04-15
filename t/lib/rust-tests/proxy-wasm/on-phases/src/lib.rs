use log::info;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::collections::HashMap;

const ROOT_ID: u32 = 0;

struct HttpHeadersRoot {
    config: Option<HashMap<String, String>>,
}

impl Context for HttpHeadersRoot {}
impl RootContext for HttpHeadersRoot {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(HttpHeaders { context_id }))
    }

    fn on_vm_start(&mut self, config_size: usize) -> bool {
        info!("#{} on_vm_start, config_size: {}", ROOT_ID, config_size);
        true
    }

    fn on_configure(&mut self, config_size: usize) -> bool {
        info!("#{} on_configure, config_size: {}", ROOT_ID, config_size);

        if let Some(config_bytes) = self.get_configuration() {
            let config_str = String::from_utf8(config_bytes).unwrap();
            self.config = Some(
                config_str
                    .split_whitespace()
                    .filter_map(|s| s.split_once('='))
                    .map(|(k, v)| (k.to_string(), v.to_string()))
                    .collect(),
            );
            info!("#{} config: {:?}", ROOT_ID, self.config.as_ref().unwrap());
        }

        true
    }
}

struct HttpHeaders {
    context_id: u32,
}

impl Context for HttpHeaders {}
impl HttpContext for HttpHeaders {
    fn on_http_request_headers(&mut self, nheaders: usize) -> Action {
        info!(
            "#{} on_request_headers, {} headers",
            self.context_id, nheaders
        );
        Action::Continue
    }

    fn on_http_response_headers(&mut self, nheaders: usize) -> Action {
        info!(
            "#{} on_response_headers, {} headers",
            self.context_id, nheaders
        );
        Action::Continue
    }

    fn on_log(&mut self) {
        info!("#{} on_done", self.context_id);
    }
}

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Info);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(HttpHeadersRoot { config: None })
    });
}

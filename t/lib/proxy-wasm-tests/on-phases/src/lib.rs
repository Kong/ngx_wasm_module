use log::info;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::collections::HashMap;
use std::time::Duration;

const ROOT_ID: u32 = 0;

struct HttpHeadersRoot {
    config: Option<HashMap<String, String>>,
    tick_period: u64,
    log_msg: String,
}

impl Context for HttpHeadersRoot {}
impl RootContext for HttpHeadersRoot {
    fn on_vm_start(&mut self, config_size: usize) -> bool {
        info!("#{} on_vm_start, config_size: {}", ROOT_ID, config_size);
        true // TODO: catch/test
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

            if let Some(period) = self.config.as_ref().unwrap().get("tick_period") {
                self.tick_period = period.parse().unwrap();
                self.set_tick_period(Duration::from_millis(self.tick_period));
            }

            self.log_msg = self
                .config
                .as_ref()
                .unwrap()
                .get("log_msg")
                .map_or(String::new(), |s| s.to_string());
        }

        true // TODO: catch/test
    }

    fn on_tick(&mut self) {
        info!("on_tick {}", self.tick_period);
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(HttpHeaders {
            context_id,
            log_msg: self.log_msg.clone(),
        }))
    }
}

struct HttpHeaders {
    context_id: u32,
    log_msg: String,
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
        info!("#{} on_log", self.context_id);

        if !self.log_msg.is_empty() {
            info!("#{} log_msg: {}", self.context_id, self.log_msg);
        }
    }
}

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Info);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(HttpHeadersRoot {
            config: None,
            tick_period: 0,
            log_msg: String::new(),
        })
    });
}

use log::info;
use proxy_wasm::{traits::*, types::*};
use std::collections::HashMap;
use std::time::Duration;

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Debug);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> { Box::new(OnTick { cancel: false }) });
}}

struct OnTick {
    cancel: bool,
}

impl Context for OnTick {}
impl RootContext for OnTick {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, _context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(OnTickHttp {}))
    }

    fn on_vm_start(&mut self, _: usize) -> bool {
        self.set_tick_period(Duration::from_millis(400));
        true
    }

    fn on_configure(&mut self, _: usize) -> bool {
        if let Some(config_bytes) = self.get_plugin_configuration() {
            let config_str = String::from_utf8(config_bytes).unwrap();
            let config: HashMap<String, String> = config_str
                .split_whitespace()
                .map(|k| (k.to_string(), "".to_string()))
                .collect();

            if config.contains_key("double_tick") {
                self.set_tick_period(Duration::from_millis(400));
            }
        }

        true
    }

    fn on_tick(&mut self) {
        info!("ticking");
    }
}

struct OnTickHttp;
impl Context for OnTickHttp {}
impl HttpContext for OnTickHttp {
    fn on_http_request_headers(&mut self, _: usize, _eof: bool) -> Action {
        info!("from request_headers");
        Action::Continue
    }
}

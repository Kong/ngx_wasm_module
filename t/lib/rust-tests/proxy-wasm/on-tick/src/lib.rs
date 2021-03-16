use chrono::{DateTime, Utc};
use log::info;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::time::Duration;

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Debug);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> { Box::new(OnTick) });
}

struct OnTick;
impl Context for OnTick {}
impl RootContext for OnTick {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(OnTickHttp { context_id }))
    }

    fn on_vm_start(&mut self, _: usize) -> bool {
        self.set_tick_period(Duration::from_millis(100));
        true
    }

    fn on_tick(&mut self) {
        let now: DateTime<Utc> = self.get_current_time().into();
        info!("Ticking at {}", now);
    }
}

struct OnTickHttp {
    context_id: u32,
}

impl Context for OnTickHttp {}
impl HttpContext for OnTickHttp {
    fn on_http_request_headers(&mut self, _: usize) -> Action {
        for (name, value) in &self.get_http_request_headers() {
            info!("#{} -> {}: {}", self.context_id, name, value);
        }

        Action::Continue
    }
}

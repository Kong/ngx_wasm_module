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
        Some(Box::new(OnTickHttp {
            _context_id: context_id,
        }))
    }

    fn on_vm_start(&mut self, _: usize) -> bool {
        self.set_tick_period(Duration::from_millis(400));
        true
    }

    fn on_tick(&mut self) {
        info!("Ticking");
    }
}

struct OnTickHttp {
    _context_id: u32,
}

impl Context for OnTickHttp {}
impl HttpContext for OnTickHttp {
    fn on_http_request_headers(&mut self, _: usize) -> Action {
        info!("from http_request_headers");
        Action::Continue
    }
}

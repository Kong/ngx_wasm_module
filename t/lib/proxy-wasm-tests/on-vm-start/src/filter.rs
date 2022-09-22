use log::info;
use proxy_wasm::{traits::*, types::*};

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Debug);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> { Box::new(OnVmStart) });
}}

struct OnVmStart;
impl Context for OnVmStart {}
impl RootContext for OnVmStart {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, _context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(OnVmStartHttp {}))
    }

    fn on_vm_start(&mut self, _: usize) -> bool {
        if let Some(config) = self.get_vm_configuration() {
            if let Ok(text) = std::str::from_utf8(&config) {
                info!("vm config: {}", text);
            } else {
                info!("can't parse vm config");
            }
        } else {
            info!("can't get vm config");
        }
        true
    }
}

struct OnVmStartHttp;
impl Context for OnVmStartHttp {}
impl HttpContext for OnVmStartHttp {}

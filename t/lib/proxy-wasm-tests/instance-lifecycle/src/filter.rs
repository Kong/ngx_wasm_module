use log::info;
use proxy_wasm::{traits::*, types::*};

static mut MY_STATIC_VARIABLE: i32 = 123000;

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Debug);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(InstanceLifecycleRoot)
    });
}}

struct InstanceLifecycleRoot;
impl Context for InstanceLifecycleRoot {}
impl RootContext for InstanceLifecycleRoot {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(InstanceLifecycleHttp { context_id }))
    }
}

struct InstanceLifecycleHttp {
    context_id: u32,
}
impl Context for InstanceLifecycleHttp {}
impl HttpContext for InstanceLifecycleHttp {
    fn on_http_request_headers(&mut self, _nheaders: usize, _eof: bool) -> Action {
        unsafe {
            MY_STATIC_VARIABLE += 1;
        }

        Action::Continue
    }

    fn on_log(&mut self) {
        let value = unsafe { MY_STATIC_VARIABLE };
        info!("#{} on_log: MY_STATIC_VARIABLE: {}", self.context_id, value);
    }
}

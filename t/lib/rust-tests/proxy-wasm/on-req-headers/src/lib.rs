use log::trace;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> { Box::new(HttpHeadersRoot) });
}

struct HttpHeadersRoot;

impl Context for HttpHeadersRoot {}

impl RootContext for HttpHeadersRoot {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(HttpHeaders { context_id }))
    }
}

struct HttpHeaders {
    context_id: u32,
}

impl Context for HttpHeaders {}

impl HttpContext for HttpHeaders {
    fn on_http_request_headers(&mut self, _: usize) -> Action {
        for (name, value) in &self.get_http_request_headers() {
            trace!("#{} -> {}: {}", self.context_id, name, value);
        }

        Action::Continue

        //match self.get_http_request_header(":path") {
        //    Some(path) if path == "/hello" => {
        //        self.send_http_response(
        //            200,
        //            vec![("Hello", "World"), ("Powered-By", "proxy-wasm")],
        //            Some(b"Hello, World!\n"),
        //        );
        //        Action::Pause
        //    }
        //    _ => Action::Continue,
        //}
    }

    fn on_log(&mut self) {
        trace!("#{} completed", self.context_id);
    }
}

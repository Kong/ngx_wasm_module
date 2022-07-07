pub mod test_http;
pub mod test_root;

use crate::*;

#[derive(Debug, Eq, PartialEq, enum_utils::FromStr)]
#[enumeration(rename_all = "snake_case")]
pub enum TestPhase {
    RequestHeaders,
    RequestBody,
    ResponseHeaders,
    ResponseBody,
    Log,
}

pub trait TestContext {
    fn get_config(&self, name: &str) -> Option<&String>;
}

impl Context for dyn TestContext {}

impl TestContext for TestRoot {
    fn get_config(&self, name: &str) -> Option<&String> {
        self.config.get(name)
    }
}

impl TestContext for TestHttp {
    fn get_config(&self, name: &str) -> Option<&String> {
        self.config.get(name)
    }
}

use crate::*;

pub struct TestRoot {
    pub config: HashMap<String, String>,
    pub n_sync_calls: usize,
}

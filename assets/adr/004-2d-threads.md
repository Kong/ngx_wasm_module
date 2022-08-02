# 2D Threads: Ergonomic multi-worker WebAssembly plugins

* Status: proposed
* Deciders: WasmX
* Date: 2022-07-29

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Problem Statement](#problem-statement)
- [Decision Drivers](#decision-drivers)
- [Considered Options](#considered-options)
- [Proposal](#proposal)
  - [User experience](#user-experience)
  - [Threading model](#threading-model)
  - [Memory mapping](#memory-mapping)
  - [Non-blocking notification: `spawn`](#non-blocking-notification-spawn)
  - [Compatibility with proxy-wasm ABI](#compatibility-with-proxy-wasm-abi)
  - [Fault recovery](#fault-recovery)
  - [Known Limitations](#known-limitations)
- [Decision Outcomes](#decision-outcomes)

## Problem Statement

Nginx is a multi-process system where there is one *master* process and multiple *worker* processes. There is a need to share data and communicate between WebAssembly plugin instances in different worker processes, but it is not currently possible to pass data across the process boundary in WasmX.

[Back to TOC](#table-of-contents)

## Decision Drivers

- Native-like user experience: Enable the use of language standard libraries where possible, instead of building custom APIs.
- Efficiency: Shared data structures and cross-worker signaling should be fast, ideally close to native multi-thread performance.

[Back to TOC](#table-of-contents)

## Considered Options

Another option is to do what the `proxy-wasm` spec defines: provide specialized functions that give access to key-value stores and queues shared across all worker processes.

For the key-value store, there are two functions defined: `proxy_get_shared_data` and `proxy_set_shared_data`. The API exposed in `proxy-wasm`'s Rust SDK looks like:

```rust
pub fn set_shared_data(key: &str, value: Option<&[u8]>, cas: Option<u32>) -> Result<(), Status> { /* ... */ }
pub fn get_shared_data(key: &str) -> Result<(Option<Bytes>, Option<u32>), Status> { /* ... */ }
```

This is a rough translation of the OpenResty Lua shdict API. But this poses several problems in comparison to a fully-fledged, "real" key-value store:

1. How can a user list the key-value pairs by prefix?
2. What kind of ordering behavior does this API expose? If it is an ordered tree-map, what if the user needs to preserve insertion order?
3. How can a user do multi-key transactions?
4. How can a user set TTL on keys?
5. How can a user implement single-key locked updates so they can prevent the thundering herd problem when using the kv store as a in-memory cache?
6. Values are plain `Bytes`. To store richly-typed data, the user needs to serialize into and deserialize from bytes. This is slow, does not work with all types, and performs at lease two extra memory copies.

Obviously it is impossible to bake all these functionalities into WasmX itself. However, at the end of the day, all these problems are **elegantly solvable in high-level languages**, provided with the required multi-thread synchronization primitives. Taking Rust as an example, there are `BTreeMap` and `HashMap` in the standard library exposing well-defined behavior, and widely-used libraries in the ecosystem - for example, [indexmap](https://crates.io/crates/indexmap), [lru](https://crates.io/crates/lru), and [moka](https://crates.io/crates/moka).

The problem with `proxy-wasm`'s queues is similar. A subset of the API exposed in `proxy-wasm`'s Rust SDK looks like:

```rust
pub fn enqueue_shared_queue(queue_id: u32, value: Option<&[u8]>) -> Result<(), Status> { /* ... */ }
pub fn dequeue_shared_queue(queue_id: u32) -> Result<Option<Bytes>, Status> { /* ... */ }
```

We can easily ask a lot of similar questions. Like:

1. How can a user apply backpressure to a queue?
2. How to choose between broadcast and single-consumer semantics?
3. The same serialization problem, as above.

Again, obviously it is impossible to bake all these functionalities into WasmX itself, but there are abundant solutions in high-level languages, provided with the required primitives.

## Proposal

This proposal introduces a design of an efficient and ergonomic process-boundary-crossing mechanism for WebAssembly called "2D Threads". **WebAssembly modules are provided with a single-process multi-threaded view of its environment** when the Nginx instance runs with multiple worker processes, without changes to Nginx's own process model.

This section introduces the following aspects of the design:

- End-user experience.
- The proposed threading model.
- Design of the memory mapping mechanism.
- Non-blocking notification.
- `proxy-wasm` ABI compatibility.
- Known limitations.

[Back to TOC](#table-of-contents)

### User experience

The user opts in to the feature by defining a symbol in their WebAssembly module. (name TBD)

```rust
#[no_mangle]
pub extern "C" fn wasmx_feature_enable_threading() {}
```

Then the user can use the data structures and synchronization primitives provided by the language's standard library and third-party libraries.

- `std::sync::mpsc` and `HashMap` / `BTreeMap` in Rust

Pseudo-code for a simplified, multi-worker HTTP caching plugin would look like:

```rust
struct HttpCache {
  cache: moka::Cache<RequestKey, Response>,
}

#[async_trait]
impl Plugin for HttpCache {
  async fn handle(&self, req: Request) -> Result<Response> {
    let request_key = req.cache_key();
    let response = self.cache.try_get_with(request_key, async {
      wasmx::http::send(req).await
    }).await?;
    Ok(response)
  }
}
```

### Threading model

This proposal introduces a "two-dimensional" threading model, as illustrated in the following diagram:

![](https://img.planet.ink/zhy/2022-07-29-16e3e9ffcfe8-f51be55e5314a597914bcebb09c56301.png)

Despite Nginx being a multi-process single-threaded application, WebAssembly modules see themselves as running in a single-process multi-threaded environment with a fixed number of threads that is equal to the number of worker processes. This is implemented with shared WebAsssembly memory and private shadow stacks, as described in the next section.

### Memory mapping

**Background**

As of now, all major languages that compile to `wasm32-wasi` including C/C++, Rust, and Go (TinyGo) generate WebAssembly modules with LLVM. The modules produced by LLVM place a shadow stack at the beginning of their linear memory. Static and heap data then follows, as illustrated in the following diagram:

![](https://img.planet.ink/zhy/2022-07-29-1800b85e203d-33713402cad504cd5d97aa4788c0f635.png)

In a multi-threaded environment, each thread needs its own shadow stack. Unfortunately, the start of the linear memory is the only safe location for placing the stack, because stack overflows at any other memory location have the risk of silently corrupting static and heap data. This is a major problem for properly running multiple threads with the "standard" set (exposed in JavaScript under the `WebAssembly` namespace) of host-side APIs, as described in [this blog post](https://rustwasm.github.io/2018/10/24/multithreading-rust-and-wasm.html).

**Solution**

But we control the host environment, and we can make each thread see its own shadow stack at the same address in the linear memory by mapping a distinct memory region as the stack for each instance of the WebAssembly virtual machine. As illustrated below:

![](https://img.planet.ink/zhy/2022-07-29-1804fe8c47e8-b1b8b62a9edb52e7d0fe31f57801c099.png)

### Non-blocking notification: `spawn`

Here's a pseudo-code example of a filter that waits for the next HTTP request.

```rust
lazy_static! {
  static INFLIGHT_REQUESTS: Mutex<Vec<Box<dyn FnOnce() + Send + Sync>>>;
}

pub struct Example {
  // ...
}

impl HttpContext for Example {
  fn on_http_request_headers(&mut self, nheaders: usize, eof: bool) -> Action {
    let requests = std::mem::replace(&mut *INFLIGHT_REQUESTS.lock().unwrap(), Vec::new());
    for req in requests {
      req();
    }
    Action::Continue
  }
}

// `then` is called after an HTTP request reaches any of the workers
fn wait_for_http_request(then: impl FnOnce()) {
  let current_thread = wasmx::thread::id();
  INFLIGHT_REQUESTS.lock().unwrap().push(Box::new(move || {
    wasmx::spawn_on(current_thread, then);
  }));
}
```

`spawn_on` sends a notification to the worker process corresponding to `current_thread`, and triggers WASM execution on its event loop. This
primitive is **enough to construct asynchronous code patterns**, including fitting into the Rust `async` ecosystem.

### Compatibility with proxy-wasm ABI

*Are there any proxy-wasm binaries that use kv/queues in the wild? If this is not widespread, maybe we should just push the proposed API instead?*

Compatibility with the proxy-wasm KV and queue ABI can be implemented with [dynamic linking](https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md). Before loading a module that uses the proxy-wasm-style ABI functions, a separate "shim" WebAssembly module that implements the proxy-wasm functions is dynamically linked with the main module.

### Fault recovery

In the event that a worker process crashes, it is possible to recover the execution state of all WebAssembly instances
in this process in a newly-spawned worker, if the crash happens outside a WebAssembly call stack.

The states that need to be fixed up are:

- **WASM globals**: Values of WASM globals can be recovered by copying out the globals into shared memory after exiting from a WASM call.
- **Host state**: WASM's assumptions about the host state can be fixed up by returning errors to asynchronous jobs like timers and HTTP dispatch.

### Known Limitations

- Sharing memory across different `.wasm` modules is not *directly* supported, due to ABI incompatibilities in high-level language constructs like `HashMap`. But this can be worked around by supporting to call functions from other modules and letting the callee do its own cross-thread operations.
- This design is based on a set of low-level Linux mechanisms. Since WASM runtimes (wasmtime, wasmer, v8) aren't aware of what we are doing, we need to "hack" into them - after initializing a WASM instance, we parse `/proc/self/maps`, unmap the region mapped by the runtime, and map our own memory. If in the future we want to get WasmX working on macOS and Windows, there may be extra work.

[Back to TOC](#table-of-contents)

## Decision Outcomes

TBD

[Back to TOC](#table-of-contents)


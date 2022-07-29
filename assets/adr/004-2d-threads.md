# 2D Threads: Ergonomic multi-worker WebAssembly plugins

* Status: proposed
* Deciders: WasmX
* Date: 2022-07-29

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Problem Statement](#problem-statement)
- [Decision Drivers](#decision-drivers)
- [Proposal](#proposal)
  - [User Flow](#user-flow)
  - [Threading model](#threading-model)
  - [Memory mapping](#memory-mapping)
  - [Non-blocking notification: `spawn`](#non-blocking-notification-spawn)
  - [Compatibility with proxy-wasm ABI](#compatibility-with-proxy-wasm-abi)
  - [Fault recovery](#fault-recovery)
  - [Implementation Steps](#implementation-steps)
  - [Known Limitations](#known-limitations)
- [Decision Outcomes](#decision-outcomes)

## Problem Statement

Nginx is a multi-process system where there is one *master* process and multiple *worker* processes. There is a need to share data and communicate between WebAssembly plugin instances in different worker processes, but it is not currently possible to pass data across the process boundary in WasmX.

`proxy-wasm` solves the problem by following [the OpenResty Lua API approach](https://github.com/openresty/lua-nginx-module#ngxshareddict) and providing a key-value style API, plus a message queue API where instances can send messages into the queue and get notified of new messages.

But WebAssembly is based on a different level of abstraction, on which the primitive unit of data is no longer richly-typed *values*, but just *machine words* stored in a plain, flat array of bytes. High-level languages such as Rust have built well-designed and optimized data structures for both ephemeral storage and multi-thread synchronization, and we should allow the user to reuse the existing language constructs, instead of fitting the Lua model into WebAssembly.

[Back to TOC](#table-of-contents)

## Decision Drivers

- Native-like user experience: Enable the use of language standard libraries where possible, instead of building custom APIs.
- Efficiency: Shared data structures and cross-worker signaling should be fast, ideally close to native multi-thread performance.

[Back to TOC](#table-of-contents)

## Proposal

This proposal introduces a design of an efficient and ergonomic process-boundary-crossing mechanism for WebAssembly called "2D Threads". **WebAssembly modules are provided with a single-process multi-threaded view of its environment** when the Nginx instance runs with multiple worker processes, without changes to Nginx's own process model.

- End-user experience of sharing data and sending messages between workers.
- The proposed threading model.
- Design of the memory mapping mechanism.
- Non-blocking notification.
- `proxy-wasm` ABI compatibility.
- Known limitations.

[Back to TOC](#table-of-contents)

### User Flow

The user opts in to the feature by defining a symbol in their WebAssembly module. (name TBD)

```rust
#[no_mangle]
pub extern "C" fn wasmx_feature_enable_threading() {}
```

Then the user can use the data structures and synchronization primitives provided by the language's standard library.

- `std::sync::mpsc` and `HashMap` / `BTreeMap` in Rust

### Threading model

This proposal introduces a "two-dimensional" threading model, as illustrated in the following diagram:

![](https://img.planet.ink/zhy/2022-07-29-16e3e9ffcfe8-f51be55e5314a597914bcebb09c56301.png)

Despite Nginx being a multi-process single-threaded application, WebAssembly modules see themselves as running in a single-process multi-threaded environment with a fixed number of threads that is equal to the number of worker processes.

Details TBD

### Memory mapping

**Background**

As of now, all major languages that compile to `wasm32-wasi` including C/C++, Rust, and Go (TinyGo) generate WebAssembly modules with LLVM. The modules produced by LLVM place a shadow stack at the beginning of their linear memory. Static and heap data then follows, as illustrated in the following diagram:

![](https://img.planet.ink/zhy/2022-07-29-1800b85e203d-33713402cad504cd5d97aa4788c0f635.png)

In a multi-threaded environment, each thread needs its own shadow stack. Unfortunately, the start of the linear memory is the only safe location for placing the stack, because stack overflows at any other memory location have the risk of silently corrupting static and heap data. This is a major problem for properly running multiple threads with the "standard" set (exposed in JavaScript under the `WebAssembly` namespace) of host-side APIs, as described in [this blog post](https://rustwasm.github.io/2018/10/24/multithreading-rust-and-wasm.html).

**Solution**

But we control the host environment, and we can make each thread see its own shadow stack at the same address in the linear memory by mapping a distinct memory region as the stack for each instance of the WebAssembly virtual machine. As illustrated below:

![](https://img.planet.ink/zhy/2022-07-29-1804fe8c47e8-b1b8b62a9edb52e7d0fe31f57801c099.png)

Details TBD

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

fn wait_for_http_request(then: impl FnOnce()) {
  let current_thread = wasmx::thread::id();
  INFLIGHT_REQUESTS.lock().unwrap().push(Box::new(move || {
    wasmx::spawn_on(current_thread, then);
  }));
}
```

Details TBD

### Compatibility with proxy-wasm ABI

*Are there any proxy-wasm binaries that use kv/queues in the wild?*

Details TBD

### Fault recovery

TBD

### Implementation Steps

TBD

### Known Limitations

TBD

[Back to TOC](#table-of-contents)

## Decision Outcomes

TBD

[Back to TOC](#table-of-contents)


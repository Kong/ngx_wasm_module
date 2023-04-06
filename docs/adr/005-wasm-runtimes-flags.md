# Exposing runtime flags as Nginx directives in WasmX

* Status: proposed
* Deciders: WasmX + Gateway teams
* Date: 2022-06-02

## Table of Contents

- [Problem Statement](#problem-statement)
- [Technical Context](#technical-context)
- [Decision Drivers](#decision-drivers)
- [Considered Options](#considered-options)
    - [1. flag directive](#1-flag-directive)
    - [2. flags directive](#2-flags-directive)
- [Decision Outcomes](#decision-outcomes)

## Problem Statement

How to expose as many Wasm runtime/compiler configuration flags as Nginx
directives as possible in a concise and logical way?

[Back to TOC](#table-of-contents)

## Technical Context

WasmX embeds either Wasmtime, Wasmer or V8 as its Wasm runtime. Each of these
runtimes provides configuration options that can be used to tweak their
behavior. Each of these runtimes also embeds a Wasm compiler, and Wasm
compilers also provide their own set of configuration flags. And not
surprisingly, a Wasm runtime might support different compilers:

* Wasmtime supports cranelift.
* Wasmer supports cranelift, llvm and singlepass.
* V8 supports liftoff and turbofan.

Although runtimes and compilers provide a substantial amount of configuration
options, WasmX is limited to use those exposed in the runtimes' C API.
The C APIs of the runtimes don't always expose all configuration options
available yet.

It should also be noted that while V8 provides a generic function (i.e.
`SetFlagsFromString`) for all its flags, Wasmer and Wasmtime provide one
function for each of their configuration flags (e.g.
`wasmtime_config_wasm_reference_types_set`). This is relevant because by
implementing something like `wasm_runtime_flag v8 flag_name flag_value`, we
could allow WasmX to support all V8 flags with minimum effort.

Finally, it is important to observe that although runtimes/compilers might be
used interchangeably to run Wasm code, each one has a different implementation
that allows for specific tweaking opportunities. As a result, a tweaking
opportunity might be present, for example, in only two of the three runtimes.

[Back to TOC](#table-of-contents)

## Decision Drivers

* Minimizes complexity of code in handling directives.
* Maximizes configuration simplicity.
* Maximizes configuration flexibility.
* Maximizes configuration conciseness.

[Back to TOC](#table-of-contents)

## Considered Options

1. Single Nginx directive receiving one flag name and value
2. Single Nginx directive receiving multiple flags.

[Back to TOC](#table-of-contents)

### 1. `flag` directive

The Nginx directive `flag` receives two arguments: `flag_name` and `value`. The
directive handler checks if the runtime supports `flag_name` and if it does, the
flag is added to the list of flags along with its value. The list of flags is
later iterated to apply each flag to the underlying runtime, when they are later
created.

```nginx
wasm {
  # flag name value;
  flag static_memory_bound_size 20000;
  flag static_memory_guard_size 10000;

  # compiler flags
  flag cranelift_opt_level "speed";
}
```

Pros:

* A single Nginx directive can handle all use cases while keeping code
  complexity low. The directive handler can delegate the call to a runtime
  specific function.
* In case of V8, this runtime-specific function could simply call the bridge
  code (`ngx_v8_set_flags`) that calls `SetFlagsFromString`.
* For `wasmtime` and `wasmer`, the handling of the flag setting can leverage a
  hash to map flags to their corresponding handler functions (which calls
  the runtime C API appropriately). If a flag is not present in the hash, it's
  not supported by the runtime and we can report it to the user.

Cons:

* Less verbose when many flags are set (e.g. commenting-out a number of flags to
  switch to another runtime).

[Back to TOC](#table-of-contents)

### 2. `flags` directive

The Nginx directive `flags` receives one string argument containing all flags
and values. Flags and values are paired as `flag=value` and separated by a
space.

The directive handler is slightly more complex than the `flag` one since it has
to parse the string argument into a list of flag names and values.

```nginx
wasm {
  # flags "f1=v1 f2=v2 ...";
  flags "static_memory_bound_size=20000 static_memory_guard_size=10000 \
         cranelift_opt_level=speed";
}
```

Pros:

* `flags` benefits from the same pros that `flag` does.
* Less verbose when many flags are set (e.g. commenting-out a single line of
  flags to switch to another runtime).

Cons:

* More work required in the directive handler to parse the string containing the
  flags.

[Back to TOC](#table-of-contents)

## Decision Outcomes

Although the directive `flag` implicates more verbosity when many flags are set,
the burden is not considered sufficient to make the additional work required by
`flags` worth.

[Back to TOC](#table-of-contents)

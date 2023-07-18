# Contributing

Hello, and welcome! Thank you for your interest in contributing to this project.

When contributing to an open-source project it is important to understand the
context surrounding said project, such as its target audience, maintenance
values, governance model, and roadmap. This document aims at outlining this
context for WasmX/ngx_wasm_module and ensuring expectations be set between
maintainers and contributors.

## Governance and maintenance

This project is maintained by the WasmX team at [Kong](https://konghq.com). We
have a commitment to producing a high-quality [open source](../LICENSE) module
extending Nginx with newer features (such as WebAssembly support). This module
is meant for use in Kong products such as the [Kong API
Gateway](https://github.com/kong/kong), but at the same time its design is not
restricted to those use-cases: this module is maintained as a general-purpose
Nginx module, of which the Kong Gateway is one possible downstream user.

With the above context in mind, please understand that our feature set, roadmap
prioritization, and PR acceptance criteria are all informed by a combination of
the above goals. This also affects our available bandwidth for outside
contributions.

## What kind of contributions are we are looking for?

1. *Bug reports* - if you hit any bugs in this module, we're eager to hear about
   them! Do not hesitate to open an issue; ideally with steps on how to
   reproduce the bug along with, if possible, a minimally-reproducible example
   (or test case).
2. *Bug fixes* - if you did hit a bug and found a way to fix it, that's even
   better! If you have a bugfix you'd like to contribute please open a PR;
   ideally with a regression test (see [DEVELOPER.md]). If you were not able to
   produce a regression test, at the very least we would like a description of
   how to reproduce the bug so we can verify it on our end. Also note that we
   might find a different way to fix the bug that is more in-line with the rest
   of the codebase, so please do not get upset if we end up applying our own fix
   to the bug you found instead of your PR -- the report of the bug is still
   greatly appreciated.
3. *Documentation feedback* - if you found a typo, missing instructions in
   [DEVELOPER.md], or any other section of the documentation requiring
   clarification, please open an issue and let us know about it. You may also
   suggest these changes in a PR, in which case we *may* also merge an updated
   version of your patch; nevertheless such feedback is greatly appreciated.
4. We do have a [significant
   roadmap](#roadmap) laid out ahead of
   us, so right now we are not actively looking for feature contributions
   outside of that roadmap nor do we have the resources to perform extensive
   code reviews on unexpected features. Also, we might just not intend to get a
   certain feature integrated. In any case please do contact us before working
   on a new feature or any other behavioral change. We encourage you to do so
   even if it is something from our roadmap, as our team may already be working
   on it and some context is likely required.

## Roadmap

This module's roadmap is documented via [GitHub
projects](https://github.com/Kong/ngx_wasm_module/projects).

Beyond our immediate roadmap, here is a non-exhaustive list of ideas WasmX
aims to explore:

- Improve Nginx's telemetry reporting.
- Improve Nginx's hot-reloading capabilities.
- Load Wasm bytecode at runtime (consider a dedicated Nginx process type?).
- Support for Envoy's [xDS
  API](https://www.envoyproxy.io/docs/envoy/latest/api/api_supported_versions).
- Consider multiplexing & routing connections at a lower level than currently
  offered by stock Nginx modules.
- Consider a build process with [Bazel](https://bazel.build/).

## TODOs

List the annotated "TODOs" in the sources and test cases with:

```
$ make todo
```

## Building from source

See [DEVELOPER.md] for developer resources on building this module from source
and other general development processes.

[DEVELOPER.md]: DEVELOPER.md

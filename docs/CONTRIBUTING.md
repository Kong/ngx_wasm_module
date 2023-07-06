# Contributing

Hello, and welcome! Thank you for your interest in contributing to this
project.

## Before you begin

As for any open-source project, when making a contribution it is important to
understand the workings of the project, such as its maintenance values and its
governance model. This document aims to outline those workings, to ensure that
both you as a contributor and we as maintainers have a positive experience.

## Governance and maintenance

This project is maintained by the WasmX team at [Kong](https://konghq.com). We
have a commitment to producing a high-quality [open source](../LICENSE) module
that integrates WebAssembly support into Nginx. This module is meant for use
in Kong products such as the [Kong API Gateway](https://github.com/kong/kong),
but at the same time its design is not restricted to those use-cases: this is
maintained as a general-purpose Nginx module, of which the Kong Gateway is only
one possible downstream user.

For those reasons, please understand that our feature set, roadmap
prioritization, and PR acceptance criteria are informed by a combination of
the above goals. This also affects our available bandwidth for outside
contributions.

## What contributions are we are looking for?

1. *Bug reports* - if you hit any bugs in this module, we're eager to
   hear about them! Do not hesitate to open an issue, ideally with steps
   on how to reproduce the bug.
2. *Bugfixes* - if you did hit a bug and found a way to fix it, that's
   even better! If you have a bugfix you'd like to contribute, please
   open a PR, ideally with a regression test. If you were not able to
   produce a regression test, at the very least we would like a description
   of how to reproduce the bug, so we can verify it on our end. Also note
   that we might find a different way to fix the bug that is more in line
   with the structure of the codebase, so please do not get upset if we
   end up applying our own fix to the bug you found instead of your PR --
   the report of the bug is still greatly appreciated.
3. We do have a [significant roadmap](https://github.com/Kong/ngx_wasm_module/projects)
   laid out ahead of us, so right now we are not actively looking for
   feature contributions outside of that roadmap, nor we have the resources
   to do extensive code reviews on unexpected features. Also, we might
   just not intend to get a certain feature integrated, so please do contact
   us before working on a feature PR. We encourage you to do so even if
   it is something from our roadmap, as our team may already be working on
   it.

## Roadmap

This module's roadmap is documented via [GitHub
projects](https://github.com/Kong/ngx_wasm_module/projects).

Beyond our immediate roadmap, here is a non-exhaustive list of ideas WasmX
aims to explore:

- Improve nginx's telemetry reporting.
- Improve nginx's hot-reloading capabilities.
- Load Wasm bytecode at runtime (consider a dedicated nginx process type?).
- Support for Envoy's [xDS
  API](https://www.envoyproxy.io/docs/envoy/latest/api/api_supported_versions).
- Consider multiplexing & routing connections at a lower level than currently
  offered by stock nginx modules.
- Consider a build process with [Bazel](https://bazel.build/).

## TODOs

List the annotated "TODOs" in the sources and test cases with:

```
$ make todo
```

## Building from source

See [DEVELOPER.md](DEVELOPER.md) for developer resources on building this module
from source and other general development processes.

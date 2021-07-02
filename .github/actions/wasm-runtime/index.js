const fs = require("fs")
const path = require("path")
const core = require("@actions/core")
const exec = require("@actions/exec")
const io = require("@actions/io")
const tc = require("@actions/tool-cache")

const DIR_WORK = path.join(process.env.HOME, "work")
const REPOSITORIES = {
    wasmtime: "https://github.com/bytecodealliance/wasmtime",
    wasmer: "https://github.com/wasmerio/wasmer",
}

async function exec_debug(cmd, args, opts = {}) {
    let out = ''

    opts.listeners = {
        stdline: (s) => {
            out += s
        },
        errline: (s) => {
            out += s
        },
    }

    await exec.exec(cmd, args, opts).catch()

    core.debug(`$ ${cmd.trim()}`)
    core.debug(out)
}

async function main() {
    let runtime = core.getInput("runtime", { required: true })
    let version = core.getInput("version", { required: true })
    let cache_key = `${runtime}`
    let dir = tc.find(cache_key, version)
    let repository = REPOSITORIES[runtime]
    let suffix = "tar.gz"

    if (repository === undefined) {
        return core.setFailed(`${runtime} is not a supported Wasm runtime`)
    }

    if (!dir) {
        if (runtime === "wasmtime") {
            switch (process.platform) {
                case "linux":
                    dir = `wasmtime-v${version}-x86_64-linux-c-api`
                    break

                case "darwin":
                    dir = `wasmtime-v${version}-x86_64-macos-c-api`
                    break

                default:
                    core.setFailed(`${os.platform()} not supported`)
            }

            version = version.replace(/^/, `v`)
            suffix = "tar.xz"

        } else if (runtime === "wasmer") {
            dir = `wasmer-${process.platform}-amd64`

        } else {
            core.setFailed("unreachable")
        }

        await io.mkdirP(DIR_WORK)

        let url = `${repository}/releases/download/${version}/${dir}.${suffix}`

        core.info(`Downloading ${runtime} release from ${url}`)
        let tar = await tc.downloadTool(url)

        core.info(`Extracting ${tar}`)
        let src = await tc.extractTar(tar, DIR_WORK, "xv")

        if (runtime === "wasmer") {
            dir = DIR_WORK

        } else {
            dir = path.join(DIR_WORK, dir)
        }

        dir = await tc.cacheDir(dir, cache_key, version)
    }

    core.exportVariable("NGX_WASM_RUNTIME", runtime)
    core.exportVariable("NGX_WASM_RUNTIME_DIR", dir)
    core.exportVariable("NGX_WASM_RUNTIME_INC", path.join(dir, "include"))
    core.exportVariable("NGX_WASM_RUNTIME_LIB", path.join(dir, "lib"))

    core.info(`NGX_WASM_RUNTIME = ${runtime}`)
    core.info(`NGX_WASM_RUNTIME_DIR = ${process.env.NGX_WASM_RUNTIME_DIR}`)
    core.info(`NGX_WASM_RUNTIME_INC = ${process.env.NGX_WASM_RUNTIME_INC}`)
    core.info(`NGX_WASM_RUNTIME_LIB = ${process.env.NGX_WASM_RUNTIME_LIB}`)
}

main().catch(err => {
    core.setFailed(`Failed to install Wasm runtime: ${err}`)
})

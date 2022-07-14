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

async function main() {
    let runtime = core.getInput("runtime", { required: true })
    let version = core.getInput("version", { required: true })
    let dir = tc.find(runtime, version)
    let suffix = "tar.gz"

    if (!dir) {
        let target

        switch (runtime) {
            case "wasmtime":
                switch (process.platform) {
                    case "linux":
                        target = `wasmtime-v${version}-x86_64-linux-c-api`
                        break
                    case "darwin":
                        target = `wasmtime-v${version}-x86_64-macos-c-api`
                        break
                    default:
                        return core.setFailed(`${os.platform()} not supported with Wasmtime`)
                }

                dir = path.join(DIR_WORK, target)
                version = version.replace(/^/, `v`)
                suffix = "tar.xz"
                break

            case "wasmer":
                dir = DIR_WORK
                target = `wasmer-${process.platform}-amd64`
                break

            case "v8":
                dir = path.join(DIR_WORK, `v8-${version}`)
                break

            default:
                return core.setFailed(`Unsupported Wasm runtime: ${runtime}`)
        }

        if (runtime === "v8") {
            await exec.exec("util/runtimes/v8.sh", [ dir, path.join(DIR_WORK, "downloads") ])
        } else {
            let repository = REPOSITORIES[runtime]
            if (repository === undefined) {
                return core.setFailed(`${runtime} is not a supported Wasm runtime`)
            }

            let url = `${repository}/releases/download/${version}/${target}.${suffix}`

            core.info(`Downloading ${runtime} release from ${url}`)
            let tar = await tc.downloadTool(url)

            core.info(`Extracting ${tar}`)
            let src = await tc.extractTar(tar, DIR_WORK, "xv")
        }

        dir = await tc.cacheDir(dir, runtime, version)
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

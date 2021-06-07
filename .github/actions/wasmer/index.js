const fs = require("fs")
const path = require("path")
const core = require("@actions/core")
const exec = require("@actions/exec")
const io = require("@actions/io")
const tc = require("@actions/tool-cache")

const WASMER_REPO = "https://github.com/wasmerio/wasmer"
const CACHE_KEY = "wasmer"
const DIR_WORK = path.join(process.env.HOME, "work")

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
    let version = core.getInput("version", { required: true })
    let dir = tc.find(CACHE_KEY, version)

    if (!dir) {
        dir = `wasmer-${process.platform}-amd64`

        await io.mkdirP(DIR_WORK)

        let tar = await tc.downloadTool(`${WASMER_REPO}/releases/download/${version}/${dir}.tar.xz`)
        let src = await tc.extractTar(tar, DIR_WORK, "x")
        dir = await tc.cacheDir(path.join(DIR_WORK, dir), CACHE_KEY, version)
    }

    core.exportVariable("NGX_WASM_RUNTIME_DIR", dir)
    core.exportVariable("NGX_WASM_RUNTIME_INC", path.join(dir, "include"))
    core.exportVariable("NGX_WASM_RUNTIME_LIB", path.join(dir, "lib"))

    core.info(`NGX_WASM_RUNTIME_DIR = ${process.env.NGX_WASM_RUNTIME_DIR}`)
    core.info(`NGX_WASM_RUNTIME_INC = ${process.env.NGX_WASM_RUNTIME_INC}`)
    core.info(`NGX_WASM_RUNTIME_LIB = ${process.env.NGX_WASM_RUNTIME_LIB}`)
}

main().catch(err => {
    core.setFailed(`Failed to install Wasmer: ${err}`)
})

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_binary")
load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

cc_binary(
    name = "alt-link",
    srcs = ["main.cc"],
    deps = [
        "//Alt-Link:alt-link-lib"
    ],
    linkopts = ["-s ASYNCIFY=1", "-s EXPORTED_RUNTIME_METHODS='[\"callMain\"]'"]
)

wasm_cc_binary(
    name = "alt-link-wasm",
    cc_target = ":alt-link",
)

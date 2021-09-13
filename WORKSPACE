load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "emsdk",
    strip_prefix = "emsdk-f9dee6673eb0692d79c45040448dfaafeb8842fb/bazel",
    url = "https://github.com/emscripten-core/emsdk/archive/f9dee6673eb0692d79c45040448dfaafeb8842fb.tar.gz",
    sha256 = "1b361257604adb9333584654a706a277a347f7f8033392e1e76dfc38451e1125",
)

load("@emsdk//:deps.bzl", emsdk_deps = "deps")
emsdk_deps()

load("@emsdk//:emscripten_deps.bzl", emsdk_emscripten_deps = "emscripten_deps")
emsdk_emscripten_deps()

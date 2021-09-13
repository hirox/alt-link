load("@rules_cc//cc:defs.bzl", "cc_library", "cc_binary")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "alt-link-lib",
    srcs = [
        "ADIv5.cpp",
        "ADIv5TI.cpp",
        "ARMv6MBPU.cpp",
        "ARMv6MDWT.cpp",
        "ARMv6MSCS.cpp",
        "ARMv7ARDIF.cpp",
        "ARMv7MFPB.cpp",
        "CMSIS-DAP.cpp",
        "Component.cpp",
        "Converter.cpp",
        "JEP106.cpp",
        "PacketTransfer.cpp",
        "RemoteSerialProtocol.cpp",
    ],
    includes = ["."],
    hdrs = glob(["*.h"]),
    deps = [
        "//cereal/include:cereal_lib",
    ],
    copts = ["-D_GNU_SOURCE"]
)

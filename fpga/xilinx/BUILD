load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@rules_license//rules:license.bzl", "license")

package(
    default_applicable_licenses = [":license"],
    default_visibility = ["//:__subpackages__"],
)

license(
    name = "license",
    package_name = "xilinx",
    license_kinds = [
        "@rules_license//licenses/spdx:ISC",
    ],
)

cc_library(
    name = "big-endian-span",
    hdrs = [
        "big-endian-span.h",
    ],
    deps = [
        "@abseil-cpp//absl/types:span",
    ],
)

cc_test(
    name = "big-endian-span_test",
    srcs = [
        "big-endian-span_test.cc",
    ],
    deps = [
        ":big-endian-span",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "bit-ops",
    hdrs = [
        "bit-ops.h",
    ],
)

cc_test(
    name = "bit-ops_test",
    srcs = [
        "bit-ops_test.cc",
    ],
    deps = [
        ":bit-ops",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "arch-xc7-frame",
    srcs = [
        "arch-xc7-frame.cc",
    ],
    hdrs = [
        "arch-xc7-frame.h",
    ],
    deps = [
        ":bit-ops",
    ],
)

cc_test(
    name = "arch-xc7-frame_test",
    srcs = [
        "arch-xc7-frame_test.cc",
    ],
    deps = [
        ":arch-xc7-frame",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "configuration-packet",
    hdrs = [
        "configuration-packet.h",
    ],
    deps = [
        "@abseil-cpp//absl/types:optional",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "arch-xc7-configuration-packet",
    srcs = [
        "arch-xc7-configuration-packet.cc",
    ],
    hdrs = [
        "arch-xc7-configuration-packet.h",
    ],
    deps = [
        ":arch-xc7-frame",
        ":bit-ops",
        ":configuration-packet",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_test(
    name = "arch-xc7-configuration-packet_test",
    srcs = [
        "arch-xc7-configuration-packet_test.cc",
    ],
    deps = [
        ":arch-xc7-configuration-packet",
        ":arch-xc7-frame",
        ":bit-ops",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "arch-xc7-part",
    srcs = [
        "arch-xc7-part.cc",
    ],
    hdrs = [
        "arch-xc7-part.h",
    ],
    deps = [
        ":arch-xc7-frame",
        "//fpga:database-parsers",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_test(
    name = "arch-xc7-part_test",
    srcs = [
        "arch-xc7-part_test.cc",
    ],
    deps = [
        ":arch-xc7-frame",
        ":arch-xc7-part",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "arch-types",
    hdrs = [
        "arch-types.h",
    ],
    deps = [
        ":arch-xc7-configuration-packet",
        ":arch-xc7-frame",
        ":arch-xc7-part",
    ],
)

cc_library(
    name = "frames",
    hdrs = [
        "frames.h",
    ],
    deps = [
        ":arch-types",
    ],
)

cc_test(
    name = "frames-xc7_test",
    srcs = [
        "frames-xc7_test.cc",
    ],
    deps = [
        ":arch-types",
        ":arch-xc7-frame",
        ":frames",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "configuration",
    srcs = [
        "configuration.cc",
    ],
    hdrs = [
        "configuration.h",
    ],
    deps = [
        ":arch-types",
        ":arch-xc7-configuration-packet",
        ":bit-ops",
        ":configuration-packet",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/types:optional",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_test(
    name = "configuration-xc7_test",
    srcs = [
        "configuration-xc7_test.cc",
    ],
    data = [
        "testdata/xc7-configuration.bit",
        "testdata/xc7-configuration.debug.bit",
        "testdata/xc7-configuration.perframecrc.bit",
        "testdata/xc7-configuration-test.json",
    ],
    deps = [
        ":arch-types",
        ":arch-xc7-frame",
        ":bitstream-reader",
        ":configuration",
        ":configuration-packet",
        ":frames",
        "//fpga:database-parsers",
        "//fpga:memory-mapped-file",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:optional",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "bitstream-reader",
    hdrs = [
        "bitstream-reader.h",
    ],
    deps = [
        ":arch-types",
        ":big-endian-span",
        "@abseil-cpp//absl/types:optional",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_test(
    name = "bitstream-reader_test",
    srcs = [
        "bitstream-reader-xc7_test.cc",
    ],
    deps = [
        ":arch-types",
        ":bitstream-reader",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "bitstream-writer",
    srcs = [
        "bitstream-writer.cc",
    ],
    hdrs = [
        "bitstream-writer.h",
    ],
    deps = [
        ":arch-types",
        ":arch-xc7-configuration-packet",
        ":bit-ops",
        ":configuration-packet",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
        "@abseil-cpp//absl/types:optional",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_test(
    name = "bitstream-writer_test",
    srcs = [
        "bitstream-writer_test.cc",
    ],
    deps = [
        ":arch-types",
        ":arch-xc7-configuration-packet",
        ":bit-ops",
        ":bitstream-writer",
        ":configuration-packet",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "bitstream",
    hdrs = [
        "bitstream.h",
    ],
    deps = [
        ":arch-types",
        ":bitstream-writer",
        ":configuration",
        ":frames",
        "//fpga:database-parsers",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

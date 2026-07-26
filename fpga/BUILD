load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_license//rules:license.bzl", "license")

package(
    default_applicable_licenses = [":license"],
    default_visibility = ["//:__subpackages__"],
)

license(
    name = "license",
    package_name = "fpga",
    license_kinds = [
        "@rules_license//licenses/spdx:Apache-2.0",
    ],
)

cc_library(
    name = "fasm-parser",
    hdrs = [
        "fasm-parser.h",
    ],
)

cc_test(
    name = "fasm-parser_test",
    srcs = [
        "fasm-parser_test.cc",
    ],
    deps = [
        ":fasm-parser",
    ],
)

cc_library(
    name = "memory-mapped-file",
    srcs = [
        "memory-mapped-file.cc",
    ],
    hdrs = [
        "memory-mapped-file.h",
    ],
    deps = [
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "database-parsers",
    srcs = [
        "database-parsers.cc",
    ],
    hdrs = [
        "database-parsers.h",
    ],
    deps = [
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@rapidjson",
    ],
)

cc_test(
    name = "database-parsers_test",
    srcs = [
        "database-parsers_test.cc",
    ],
    deps = [
        ":database-parsers",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:str_format",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "database",
    srcs = [
        "database.cc",
    ],
    hdrs = [
        "database.h",
    ],
    deps = [
        ":database-parsers",
        ":memory-mapped-file",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_test(
    name = "database_test",
    srcs = [
        "database_test.cc",
    ],
    deps = [
        ":database",
        ":database-parsers",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_binary(
    name = "fpga-as",
    srcs = [
        "assembler.cc",
    ],
    deps = [
        ":database",
        ":database-parsers",
        ":fasm-parser",
        "//fpga/xilinx:arch-types",
        "//fpga/xilinx:bitstream",
        "@abseil-cpp//absl/cleanup:cleanup",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

workspace(name = "thornmarked")

load("//base/bazel:tools.bzl", "local_tools_repository")

local_tools_repository(
    name = "tools",
    tools = [
        "flac",
        "sox",
    ],
)

load("//base/bazel:pkg_config.bzl", "pkg_config_repository")

pkg_config_repository(
    name = "freetype",
    includes = [
        "freetype/**/*.h",
        "ft2build.h",
    ],
    spec = "freetype2",
)

pkg_config_repository(
    name = "assimp",
    includes = [
        "assimp/**/*.h",
        "assimp/**/*.hpp",
        "assimp/**/*.inl",
    ],
    spec = "assimp",
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "io_bazel_rules_go",
    sha256 = "ac03931e56c3b229c145f1a8b2a2ad3e8d8f1af57e43ef28a26123362a1e3c7e",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.24.4/rules_go-v0.24.4.tar.gz",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.24.4/rules_go-v0.24.4.tar.gz",
    ],
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains()

register_toolchains(
    "//n64:cc-toolchain-n64",
)

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
    name = "fmt",
    build_file = "@//third_party/fmt:fmt.bazel",
    sha256 = "4119a1c34dff91631e1d0a3707428f764f1ea22fe3cd5e70af5b4ccd5513831c",
    strip_prefix = "fmt-7.1.2",
    urls = [
        "https://github.com/fmtlib/fmt/archive/7.1.2.tar.gz",
    ],
)

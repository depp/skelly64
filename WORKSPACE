workspace(name = "com_github_depp_skelly64")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# ==============================================================================
# Local Dependencies
# ==============================================================================

load("//bazel:pkg_config.bzl", "pkg_config_repository")

pkg_config_repository(
    name = "assimp",
    includes = [
        "assimp/**/*.h",
        "assimp/**/*.hpp",
        "assimp/**/*.inl",
    ],
    spec = "assimp",
)

# ==============================================================================
# Go
# ==============================================================================

http_archive(
    name = "io_bazel_rules_go",
    sha256 = "f2dcd210c7095febe54b804bb1cd3a58fe8435a909db2ec04e31542631cf715c",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.31.0/rules_go-v0.31.0.zip",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.31.0/rules_go-v0.31.0.zip",
    ],
)

http_archive(
    name = "bazel_gazelle",
    sha256 = "5982e5463f171da99e3bdaeff8c0f48283a7a5f396ec5282910b9e8a49c0dd7e",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.25.0/bazel-gazelle-v0.25.0.tar.gz",
        "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.25.0/bazel-gazelle-v0.25.0.tar.gz",
    ],
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(version = "1.18.2")

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

go_repository(
    name = "org_golang_x_text",
    importpath = "golang.org/x/text",
    sum = "h1:olpwvP2KacW1ZWvsR7uQhoyTYvKAupfQrRGBFM352Gk=",
    version = "v0.3.7",
)

go_repository(
    name = "org_golang_x_tools",
    importpath = "golang.org/x/tools",
    sum = "h1:FDhOuMEY4JVRztM/gsbk+IKUQ8kj74bxZrgw87eMMVc=",
    version = "v0.0.0-20180917221912-90fa682c2a6e",
)

gazelle_dependencies()

# ==============================================================================
# Built Dependencies
# ==============================================================================

http_archive(
    name = "fmt",
    build_file = "@//third_party/fmt:fmt.bazel",
    sha256 = "23778bad8edba12d76e4075da06db591f3b0e3c6c04928ced4a7282ca3400e5d",
    strip_prefix = "fmt-8.1.1",
    urls = [
        "https://github.com/fmtlib/fmt/releases/download/8.1.1/fmt-8.1.1.zip",
    ],
)

http_archive(
    name = "freetype",
    build_file = "@//third_party/freetype:freetype.bazel",
    patch_args = ["-p1"],
    patches = [
        "//third_party/freetype:freetype-2.11.0.patch",
    ],
    sha256 = "8bee39bd3968c4804b70614a0a3ad597299ad0e824bc8aad5ce8aaf48067bde7",
    strip_prefix = "freetype-2.11.0",
    type = "tar.xz",
    urls = [
        "https://sourceforge.net/projects/freetype/files/freetype2/2.11.0/freetype-2.11.0.tar.xz/download",
        "https://download.savannah.gnu.org/releases/freetype/freetype-2.11.0.tar.xz",
    ],
)

http_archive(
    name = "zlib",
    build_file = "@//third_party/zlib:zlib.bazel",
    sha256 = "4ff941449631ace0d4d203e3483be9dbc9da454084111f97ea0a2114e19bf066",
    strip_prefix = "zlib-1.2.11",
    urls = [
        "https://prdownloads.sourceforge.net/libpng/zlib-1.2.11.tar.xz?download",
        "https://zlib.net/zlib-1.2.11.tar.xz",
    ],
)

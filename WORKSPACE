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
    sha256 = "ac03931e56c3b229c145f1a8b2a2ad3e8d8f1af57e43ef28a26123362a1e3c7e",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.24.4/rules_go-v0.24.4.tar.gz",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.24.4/rules_go-v0.24.4.tar.gz",
    ],
)

http_archive(
    name = "bazel_gazelle",
    sha256 = "b85f48fa105c4403326e9525ad2b2cc437babaa6e15a3fc0b1dbab0ab064bc7c",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.22.2/bazel-gazelle-v0.22.2.tar.gz",
        "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.22.2/bazel-gazelle-v0.22.2.tar.gz",
    ],
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains()

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

go_repository(
    name = "org_golang_x_text",
    importpath = "golang.org/x/text",
    sum = "h1:aRYxNxv6iGQlyVaZmk6ZgYEDa+Jg18DxebPSrd6bg1M=",
    version = "v0.3.6",
)

go_repository(
    name = "org_golang_x_tools",
    importpath = "golang.org/x/tools",
    sum = "h1:n7NCudcB/nEzxVGmLbDWY5pfWTLqBcC2KZ6jyYvM4mQ=",
    version = "v0.0.0-20180917221912-90fa682c2a6e",
)

gazelle_dependencies()

# ==============================================================================
# Built Dependencies
# ==============================================================================

http_archive(
    name = "fmt",
    build_file = "@//third_party/fmt:fmt.bazel",
    sha256 = "5cae7072042b3043e12d53d50ef404bbb76949dad1de368d7f993a15c8c05ecc",
    strip_prefix = "fmt-7.1.3",
    urls = [
        "https://github.com/fmtlib/fmt/archive/7.1.3.tar.gz",
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

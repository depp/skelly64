workspace(name = "thornmarked")

load("//base/bazel:pkg_config.bzl", "pkg_config_repository")

pkg_config_repository(
    name = "freetype",
    spec = "freetype2",
    includes = [
        "ft2build.h",
        "freetype/**/*.h",
    ],
)

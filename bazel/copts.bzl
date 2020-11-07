# Bazel + GCC,
#   Default:
#     -U_FORTIFY_SOURCE
#     -Wall
#     -Wunused-but-set-parameter
#     -Wno-free-nonheap-object
#     -fno-omit-frame-pointer
#   With -c dbg, adds:
#     -g
#   With -c opt, adds:
#     -g0 -O2

# Base C options
_COPTS_BASE = [
    "-std=c11",
    "-D_DEFAULT_SOURCE",
]

_COPTS_WARNING = _COPTS_BASE + [
    "-Wall",
    "-Wextra",
    "-Wpointer-arith",
    "-Wwrite-strings",
    "-Wmissing-prototypes",
    "-Wdouble-promotion",
    "-Werror=implicit-function-declaration",
    "-Winit-self",
    "-Wstrict-prototypes",
    "-Wno-format-zero-length",
]

# Internal C compilation options. Use this by default for all C targets in the
# repo.
COPTS = (
    _COPTS_BASE +
    select({
        "//base/bazel:warnings_off": [],
        "//base/bazel:warnings_on": _COPTS_WARNING,
        "//base/bazel:warnings_error": _COPTS_WARNING + ["-Werror"],
    })
)

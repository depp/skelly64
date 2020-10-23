package(default_visibility = ["//visibility:public"])

cc_library(
    name = %{name},
    hdrs = %{hdrs},
    defines = %{defines},
    includes = %{includes},
    linkopts = %{linkopts},
)

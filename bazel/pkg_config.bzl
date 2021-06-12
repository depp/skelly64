_DEFAULT_TEMPLATE = Label("@thornmarked//bazel:pkg_config.BUILD.tpl")

def _run_pkg_config(repository_ctx, spec, flags):
    exec_result = repository_ctx.execute(["pkg-config"] + flags + [spec])
    if exec_result.return_code != 0:
        fail("Could not find package " + spec)
    return [flag for flag in exec_result.stdout.strip().split(" ") if flag]

def _impl(repository_ctx):
    spec = repository_ctx.attr.spec
    includes = repository_ctx.attr.includes
    build_file_template = repository_ctx.attr.build_file_template

    cflags = _run_pkg_config(repository_ctx, spec, ["--cflags"])
    libs = _run_pkg_config(repository_ctx, spec, ["--libs"])

    linkopts = []
    for flag in libs:
        if flag.startswith("-l") or flag.startswith("-L"):
            linkopts.append(flag)
        else:
            fail("Cannot parse linker flag " + repr(flag) + " for " + spec)

    defines = []
    include_paths = []
    include_root = repository_ctx.path("include")
    for flag in cflags:
        if flag.startswith("-I"):
            # Create a symlink in the directory and add *.h files in it to hdrs.
            path = flag[2:]
            if not path.startswith("/"):
                fail("Include path %r is not absolute" % path)
            dir_name = path.replace("/", "_")
            repository_ctx.symlink(
                repository_ctx.path(path),
                include_root.get_child(dir_name),
            )
            include_paths.append("include/" + dir_name)
        elif flag.startswith("-D"):
            defines.append(flag[2:])
        else:
            fail("Cannot parse compiler flag %r for %s" % (flag, spec))

    hdrs = "[]"
    if includes:
        hdrs = "glob(%s)" % repr([
            "include/*/" + include
            for include in includes
        ])

    substitutions = {
        "%{name}": repr(repository_ctx.name),
        "%{hdrs}": hdrs,
        "%{defines}": repr(defines),
        "%{includes}": repr(include_paths),
        "%{linkopts}": repr(linkopts),
    }
    repository_ctx.template("BUILD.bazel", build_file_template, substitutions)

pkg_config_repository = repository_rule(
    implementation = _impl,
    local = True,
    attrs = {
        "spec": attr.string(mandatory = True),
        "includes": attr.string_list(),
        "build_file_template": attr.label(
            default = _DEFAULT_TEMPLATE,
            allow_files = True,
        ),
    },
)

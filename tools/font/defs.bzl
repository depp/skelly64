def _bitmap_font_impl(ctx):
    charset = ctx.file.charset
    args = [
        "-size=" + ctx.attr.font_size,
        "-format=" + ctx.attr.format,
        "-texture-size=" + ctx.attr.texsize,
        "-remove-notdef",
        "-charset=" + charset.path,
    ]
    if ctx.attr.shadow:
        args.append("-shadow=" + ctx.attr.shadow)
    if ctx.attr.encoding:
        args.append("-encoding=" + ctx.attr.encoding)
    if ctx.attr.case:
        args.append("-case=" + ctx.attr.case)
    src = ctx.file.src
    out_dat = ctx.actions.declare_file(ctx.label.name + ".font")
    out_tex = ctx.actions.declare_file(ctx.label.name + ".png")
    ctx.actions.run(
        outputs = [out_dat, out_tex],
        inputs = [src, charset],
        progress_message = "Converting font %s" % src.short_path,
        executable = ctx.executable._converter,
        arguments = [
            "-font=" + src.path,
            "-out-texture=" + out_tex.path,
            "-out-data=" + out_dat.path,
        ] + args,
    )
    return [DefaultInfo(files = depset([out_dat]))]

bitmap_font = rule(
    implementation = _bitmap_font_impl,
    attrs = {
        "src": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "font_size": attr.string(
            mandatory = True,
        ),
        "format": attr.string(
            mandatory = True,
        ),
        "texsize": attr.string(
            mandatory = True,
        ),
        "charset": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "encoding": attr.string(),
        "shadow": attr.string(),
        "case": attr.string(),
        "_converter": attr.label(
            default = Label("//tools/font"),
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
)

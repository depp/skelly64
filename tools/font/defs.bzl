def _bitmap_font_impl(ctx):
    src = ctx.file.src
    charset = ctx.file.charset
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
            "-size=" + ctx.attr.font_size,
            "-format=" + ctx.attr.format,
            "-texture-size=" + ctx.attr.texsize,
            "-remove-notdef",
            "-charset=" + charset.path,
        ],
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
        "_converter": attr.label(
            default = Label("//tools/font"),
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
)

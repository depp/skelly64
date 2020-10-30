def bitmap_font(name, src, size, format, texsize, charset):
    native.genrule(
        name = name,
        srcs = [src, charset],
        outs = [name + ".font", name + ".png"],
        tools = ["//tools/font", "//tools/font/raster"],
        cmd = (
            "$(location //tools/font) " +
            "-tool-raster=$(location //tools/font/raster) " +
            "-font=$(location %s) " % src +
            "-out-texture=$(location %s.png) " % name +
            "-out-data=$(location %s.font) " % name +
            "-size=%d " % size +
            "-format=%s " % format +
            "-texture-size=%d:%d " % texsize +
            "-remove-notdef " +
            "-charset=$(location %s)" % charset
        ),
    )

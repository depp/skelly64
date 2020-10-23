# Font Tools

Create pixel fonts. You might do this:

    $ bazel run :font --
        -font <path-to-font>
        -size=16
        -out-grid=out.png
        -out-texture=tex.png
        -texture-size=64:64
        -charset=20,30-39,41-5a,61-7a
        -remove-notdef

This does the following:

- Renders the font at 16 pixels high

- Removes all characters except space and alphanumerics, and removes the ".notdef" glyph

- Puts all glyphs in an easy-to-read file, out.png

- Packs glyphs tightly into tex.png, which consists of one or more 64x64 sections

# Bitmap Font Example

This directory contains a demo for how to create a packed monospace bitmap font that can be used on the Nintendo 64. See [Bitmap Example][bitmap_example] in the Skelly 64 documentation for a more detailed discussion of this example.

The font in this directory is a subset of [Terminus][terminus], a clean bitmap font which is fantastic for consoles and programming at low resolution.

[bitmap_example]: https://depp.github.io/skelly64/font/example_bitmap/
[terminus]: http://terminus-font.sourceforge.net/

## Building the Fallback Font

### Bazel

Run the following command in this directory:

```shell
bazel build :terminus
```

This will create a file named `ter-u12n.h` in the folder `../../bazel-bin/examples/font_bitmap`.

## Maintainer Notes

This information is only relevant to the Skelly 64 maintainers.

To keep the repository small, the Terminus font in this directory is a subset of the full font. It was created by editing the font file manually: deleting all non-ASCII characters besides U+FFFD, and updating the `CHARS` value.

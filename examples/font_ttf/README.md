# TTF Font Example

This directory contains a demo for how to create a packed bitmap font from a normal TTF outline font.

## Building the Bitmap Font

### Bazel

Run the following command in this directory:

```shell
bazel build :alegreya
```

This will create a file named `alegreya.font` and `alegreya.png` in the folder `../../bazel-bin/examples/font_ttf`,

## Maintainer Notes

This information is only relevant to the Skelly 64 maintainers.

To keep the repository small, the Alegray font in this directory is a subset of the full font. Using `pyftsubset`:

```shell
python -m pip install fonttools
pyftsubset Alegreya-Medium.ttf \
    --output-file=Alegreya-Medium.subset.ttf \
    --layout-features= \
    --unicodes=U+0000-00FF,U+2000-206F,U+FFFD \
    --no-hinting \
    --desubroutinize
```

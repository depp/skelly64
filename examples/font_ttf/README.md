# TTF Font Example

This directory contains a demo for how to create a packed bitmap font from a normal TTF outline font.

## Font Subset

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

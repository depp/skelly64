# Skelly 64

Tools for Nintendo 64 homebrew game development.

This is somewhat a work in progress.

Note that if you are looking for **ROM hacking** tools, Skelly 64 won’t help you. ROM hacking is just too different from homebrew development.

## Tools

### Audio Converter

**Currently broken.**

### Font Packer

Converts a font to a bitmap that you can use on the Nintendo 64. Ordinary font files, like TrueType files, are rasterized (converted to bitmaps) and packed into textures.

### Image Converter

Converts an image to formats that you can use on the Nintendo 64.

- Converts sRGB to linear RGB.

- Creates mipmaps for textures.

- Slices 2D images into strips that each fit into TMEM.

### Model Converter

Converts

### ROM Creator

Creates the bootsector of an Nintendo 64 ROM image.

### Texture Display List Creator

Creates a display list for mip-mapped textures.
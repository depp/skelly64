# Skelly 64

Tools for Nintendo 64 homebrew game development.

This is a **work in progress.** If you want to use these tools, you will need to be able to able to figure things out by reading the source code.

Note that if you are looking for **ROM hacking** tools, Skelly 64 won’t help you. ROM hacking is just too different from homebrew development.

Skelly 64 is licensed under the terms of the Mozilla Public License Version 2.0. See LICENSE.txt for details.

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

## Building

Building requires the following prerequisites:

- [Bazel](https://bazel.build/) 4.1.0 (You can try other versions, but you will need to change the .bazelversion file in order for other versions to work.)
- [Pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
- [SoX](http://sox.sourceforge.net/)
- [AssImp](https://www.assimp.org/)

### Development

If you are making changes to Skelly 64, you should enable C++ compiler warnings. The easiest way to do this is by adding a file named `.user.bazelrc` to the top-level directory, containing the following configuration:

    build --//bazel:warnings=error

To update the Go dependencies:

    go get -u ./...
    go mod tidy
    bazel run //:gazelle -- -from_file=go.mod

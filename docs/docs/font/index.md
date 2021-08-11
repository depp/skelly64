# Font Builder

The font builder converts fonts to a format which can be used by the Nintendo 64.

## Features

- Supports most font formats. Uses the [FreeType][freetype] library, which supports common formats like TTF and OTF, as well as more obscure formats like BDF.

- Produces two types of fonts. “Texture fonts” are fonts designed to be loaded into texture memory (TMEM) on the RDP, and drawn by the RDP. “Fallback fonts” are designed to be drawn by the CPU, for example, if you want to draw something on-screen from a crash handler.

- Includes only a subset of the glyphs in a font. You can choose which glyphs that you want, like A-Z, 0-9, and the punctuation you need. This reduces the amount of data you have to load into TMEM.

- Supports different character encodings. You can use Unicode, or a more convenient encoding like Windows-1252 or Latin-1.

- Adds shadows to the font, if desired.

[freetype]: https://www.freetype.org/

## Running

```shell
font -font <input> -font-size <size>
     [-out-data <output.font>]
     [<option>...]
```

### Font Options

These options control how fonts are loaded, rendered to bitmaps, and transformed.

- `-case <upper|lower>`: Transform all text to upper-case or lower-case.

- `-charset <file>`: Only use a subset of the font, including characters from a character set file.

- `-encoding <name>`: Specify the character encoding. Only simple 8-bit character encodings are supported. By default, the glyphs are encoded as their Unicode code points.

- `-font <input>`: Read the font from `<input>`. Most font formats supported by the FreeType library should work. TrueType (TTF) has been tested and is known to work, as well as Adobe BDF.

- `-font-size <size>`: Render the font at `<size>` pixels.

- `-mono`: Render the font outline as monochrome (1-bit), with no antialiasing.

- `-out-grid <file.png>`: After loading the font and applying all transformations, arrange the glyphs in a grid and write it to `<file.png>`.

- `-remove-notdef`: Remove the `.notdef` glyph from the font, used for missing or undefined glyphs. When specified, missing or undefined glyphs will skipped, as if they were not present in the input. Otherwise, they will usually be rendered as little squares.

- `-shadow <x>:<y>[:<alpha>]`: Bake a drop shadow into the font texture. Increasing X values move the shadow farther right, and increasing Y values move the shadow farther down. Alpha should be between 0.0 and 1.0, and defaults to 1.0.

### Texture Font Output

These options control how texture fonts are produced. Texture fonts are intended to be loaded into texture memory in the Nintendo 64’s RDP, and rendered using RDP commands (either using RSP microcode, or by issuing RDP commands directly).

- `-format <format>`: Use `<format>` as the pixel format for the texture. This must be a texture format that the RDP supports (note: CI is not currently supported).

- `-out-data <out.font>`: Write the bitmap font out to a data file, using the Skelly 64 font file.

- `-out-texture <out.png>`: Combine the font’s textures into a single PNG file, and write it out. Used for previewing what the glyphs look like, and seeing how they are packed.

- `-texture-size <width>:<height>`: Specify the size of each texture.

### Fallback Font Output

- `-out-fallback <file.h>`: Write the fallback font as C source code to `<file.h>`.

### Texture Formats

The tool supports nine different texture formats, listed below:

| Bits | RGBA    | CI   | IA    | I   |
| ---- | ------- | ---- | ----- | --- |
| 32   | rgba.32 |      |       |     |
| 16   | rgba.16 |      | ia.16 |     |
| 8    |         | ci.8 | ia.8  | i.8 |
| 4    |         | ci.4 | ia.4  | i.4 |

The format can be chosen with the `-format` option. Note that there is no meaningful way to use color index formats at the moment.

## Charset

!!! note

    Missing documentation—how to specify the charset.

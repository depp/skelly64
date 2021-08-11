# Model Converter

!!! warning

    Missing documentation.

The model converter converts models from standard formats like FBX into formats suitable for the Nintendo 64 homebrew projects.

The Bazel target is `//tools/model`.

## Running

```
model -model <input> -output <output.model>
```

### Options

- `-animate`: Convert animations.
- `-axes <axes>`: Change the axes of the 3D model. This can be used to convert between left-handed and right-handed systems, or change which axis a model is facing towards. Defaults to `x,y,z`. (TODO: how does this work?)
- `-meter <expr>`: Define the length of a meter. The meter can be used by the `-scale` flag. The length can be a number or a simple numerical expression, such as `-meter 100/64`.
- `-model <input>`: Use `<input>` as the input model. The input may be an FBX model. Other model formats may work, but are not tested.
- `-output <output.model>`: Write the model to `<output.model>`. The output is a custom format.
- `-output-c <output.c>`: Write the model as C source code to `<output.c>`. This may not work correctly and is not intended to be used in real games, but it shows the GBI commands used in the output model.
- `-output-stats <output.log>`: Write information about the model to `<output.log>`. This information is human-readable and should not be parsed.
- `-scale <expr>`: Scale the model by this factor. The factor can be a number or a numerical expression, and it can be defined in terms of the value for the `-meter` flag. For example, `-scale 64/300` or `-scale "meter*10"`.
- `-texcoord-bits <num>`: Set the number of fractional bits of precision used for texture coordinates. Defaults to 11.
- `-use-normals`: Use vertex normals from model. Cannot be combined with vertex colors.
- `-use-primitive-color`: Use primitive color from material. (TODO: What part of the material?)
- `-use-texcoords`: Include texture coordinates.
- `-use-vertex-colors`: Include vertex colors. Cannot be combined with normals.
- `-variable-name <name>`: Use `<name>` as the name of the variable which contains the model data generated with `-output-c`.

## Building

To build,

```shell
bazel build -c opt //tools/model
```

This will place the tool in `bazel-bin/tools/model/model`.

## Input Formats

The converter supports the FBX format for input.

!!! note

    The model converter uses the [Open Asset Import Library][assimp], and in theory, can use other formats that the Open Asset Import Library supports. However, **this is not tested.**

[assimp]: https://www.assimp.org/

!!! note

    In the future, glTF may become the recommended format.

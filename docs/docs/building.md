# Building

Skelly 64 is distributed as source code, you will need to compile the tools in order to use them.

## Prerequisites

- [Bazel](https://bazel.build/) 4.1.0. Newer version should also work.
- [Pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/), used to find Assimp.
- [SoX](http://sox.sourceforge.net/), used to convert audio data.
- [Assimp](https://www.assimp.org/), used to import 3D models.

### Debian

To install the prerequisites on Debian, run:

```shell
sudo apt install build-essential sox libassimp-dev
```

### macOS (Homebrew)

To install the prerequisites using [Homebrew][brew] on macOS:

[brew]: https://brew.sh/

```shell
brew install bazel pkg-config sox assimp
```

## Building and Running

You can build one of the tools by running Bazel. The first time you build a tool with Bazel, it will download some additional prerequisites, so you will need an active internet connection.

For example, to build the font tool:

```shell
bazel build -c opt //tools/font
```

The `-c opt` tells Bazel to use the “optimized build” compilation mode, which enables compiler optimizations. This is recommended if you are just using the tools and not modifying them. Bazel will rebuild if you change this flag.

You can build and run a tool with Bazel’s `run` command:

```shell
bazel run -c opt //tools/font
```

To pass arguments to a tool, add `--` to the end of the command, and put the arguments there (otherwise, Bazel will intercept the arguments). For example, to pass the `-help` flag to the font tool:

```shell
bazel run -c opt //tools/font -- -help
```

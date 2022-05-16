# VADPCM Audio

!!! info

    The VADPCM audio converter is **fully functional** but the API and command-line interface are subject to change.

VADPCM is a simple lossy audio compression codec used by Nintendo 64 games. VADPCM is one of two audio formats supported by the official Nintendo 64 SDK, and support for VADPCM is being added to LibDragon as well.

## Tools and Libraries

The `vadpcm` tool encodes and decodes VADPCM audio from the command-line. To build it,

```
bazel build -c opt //tools/vadpcm
```

This will place the built executable at `bazel-bin/tools/vadpcm/vadpcm`.

The encoding and decoding library is found in `lib/vadpcm`.

## Background

In retail Nintendo 64 games, cartridge space was expensive, so VADPCM was primarily used for things like sound effects and musical instrument samples for MIDI playback. For homebrew games, cartridge space is much cheaper, and you can more easily justify using VADPCM to encode your entire soundtrack.

At least one retail Nintendo 64 game is known to use VADPCM audio for its soundtrack: _Star Wars, Shadows of the Empire_. From an article in [Game Developer, April 2009](https://www.gamedeveloper.com/console/classic-postmortem-i-star-wars-shadows-of-the-empire-i-),

> At this point, we tried an experiment using uncompressed digital samples of the Star Wars main theme. The quality was extremely good, even after subsequent compression with the ADPCM encoder provided by Nintendo. After a little persuasion, Nintendo generously agreed to increase the amount of cartridge space from 8MB to 12MB.

> This allowed us to include approximately 15 minutes of 16-bit, 11khz, mono music that sounded surprisingly good. Considering that most users would listen to the music through their televisions (rather than a sophisticated audio system), the results were close to that of an audio CD, thereby justifying the extra cartridge space required.

## Bit Rate

Audio encoded in VADPCM uses exactly 4.5 bits per sample. The bit rate can only be changed by using a different sample rate.

Here are some common sample rates, and the amount of data that VADPCM uses at each sample rate:

| Sample Rate | Bit Rate    | Length per Megabyte |
| ----------- | ----------- | ------------------- |
| 44.1 kHz    | 198 kbit/s  | 42.3 s/MiB          |
| 32 kHz      | 144 kbit/s  | 58.3 s/MiB          |
| 22.05 kHz   | 99.2 kbit/s | 84.5 s/MiB          |
| 16 kHz      | 72.0 kbit/s | 117 s/MiB           |
| 11.025 kHz  | 49.6 kbit/s | 169 s/MiB           |

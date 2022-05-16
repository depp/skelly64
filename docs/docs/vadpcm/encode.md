# Encode VADPCM

Usage:

```
vadpcm encode [-predictors=<N>] <input> <output.aifc>
```

## Description

Encodes an audio file as VADPCM. The input audio must be a 16-bit monophonic AIFF file.

## Options

`-predictors=<N>`

: The VADPCM codec uses a codebook containing 1-16 predictors. This option sets the number of predictors to N. Using more predictors may improve the audio quality somewhat. The default number of predictors is 4, and each predictor takes up 32 bytes of space in the codebook.

`-show-stats`

: After encoding, compare the encoded audio to the original audio and calculate the amount of noise introduced by the encoder. Prints out the signal level, noise level, and signal-to-noise ratio in dB.

`-stats-out=<file.json>`

: After encoding, compare the encoded audio to the original audio and write the signal level and noise level to a JSON file. The JSON file will represent an object with the following keys:

    - `signalLevel` - The RMS level of the original audio, a number in the range 0.0-1.0.

    - `noiseLevel` - The RMS level of the noise introduced by the encoder, a number in the range 0.0-1.0.

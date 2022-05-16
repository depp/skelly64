# VADPCM Codec

This page describes the VADPCM codec in detail. With this information, you should be able to write your own VADPCM decoder.

Writing an encoder is more difficult.

## Theory of Operation

VADPCM uses _linear predictors_ to predict what each sample of audio will be, based on the previous samples. The residual difference between the predicted value and the actual value is quantized to 4 bits. Each encoded frame of audio contains 16 samples with the same predictor coefficients and residual quantization.

In theory, you decode a sample in two steps:

1. Predict the sample value from a linear combination of previous output samples.

1. Add the residual value to the predicted value.

The actual decoding process is different, and is easy to vectorize using SIMD operations—this may be where the “V” in “VADPCM” comes from.

## Encoded Data

VADPCM audio consists of two parts: the **codebook** (which contains predictors) and the **audio data**.

### Codebook

The codebook contains the parameters necessary for decoding the associated audio data. A codebook has a predictor order, a predictor count, and an array of predictor vectors.

**Predictor Order**

: The predictor order can theoretically be any number from 0 to 8, but in practice, it is always 2. This value is the order of the linear predictors in the codebook. The predictor order is limited by the size of the array used to track decoder state in the decoder, which is 8.

**Predictor Count**

: The predictor count is just the number of predictors in the codebook. This can be any number from 1 to 16. It is limited by the number of bits used to encode predictors in the encoded audio data.

**Predictor Vectors**

: The predictor vectors are vectors of 8 signed 16-bit numbers. These are pre-calculated values derived during the encoding process. The number of vectors is equal to the predictor order, multiplied by the number of predictors.

### Audio Data

The audio data is packed into frames. Each frame is 8 bytes and contains 9 samples of audio data, giving exactly 4.5 bits per sample. A frame contains a scaling factor, a predictor index, and residual audio data.

**Scaling Factor**

: The scaling factor is a number in the range 0-12. This is used to scale the residual audio data. During decoding, the residual audio data is shifted left by the scaling factor. Values larger than 12 would result in overflow when shifting.

**Predictor Index**

: The predictor index is a number 0-15, which is a reference to a set of predictor vectors in the codebook. The predictor vectors are used to predict the value of future audio samples from the previous output.

**Residual Audio Data**

: The residual audio data is an array of 16 four-bit numbers. This contains the difference between the predicted sample data and the actual sample data, scaled down by the scaling factor.

## Decoding Process

Decoding is done in vectors of 8 samples each. Each frame contains two vectors with a shared scaling factor and predictor index. Correctly decoding a frame requires the previous frame’s decoded output, which serves as the decoder state. The initial state is all zeroes. The process for decoding a vector is as follows:

1. Initialize an accumulator to all zeroes.

1. For a predictor order of K, multiply each of the K predictor vectors by the corresponding last K output samples, and add them to the accumulator.

1. For each index in the output vector I = 1..8,

    1. Calculate the predicted output sample by shifting the corresponding element in the accumulator to the right by 11 bits.

    1. Scale the residual value by shifting it left by the frame’s scaling factor.

    1. Add the scaled residual value and the predicted output sample. This is the output sample.

    1. Shift the Kth predictor vector to the right by I elements, multiply this vector by the scaled residual value, and add this vector to the accumulator.

In C, you can decode a vector of VADPCM like this:

```c
void decode(int predictor_order,
            int scaling_factor,
            int predictor[predictor_order][8],
            int state[8],
            int residual[8],
            int output[8]) {

  // Zero the accumulator.
  int accumulator[8];
  for (int i = 0; i < 8; i++) {
    accumulator[i] = 0;
  }

  // Add previous output to accumulator.
  for (int i = 0; i < predictor_order; i++) {
    int previous_output = state[8 - predictor_order + i];
    for (int j = 0; j < 8; j++) {
      accumulator[j] += predictor[i][j] * previous_output;
    }
  }

  // Calculate each output sample, and update the accumulator.
  for (int i = 0; i < 8; i++) {
    int scaled_residual = residual[i] << scaling_factor;
    output[i] = (accumulator[i] >> 11) + scaled_residual;
    for (int j = 0; j < 7 - i; j++) {
      accumulator[i + 1 + j] +=
          predictor[predictor_order - 1][j] * scaled_residual;
    }
  }

  // New decoder state is equal to the output.
  for (int i = 0; i < 8; i++) {
    state[i] = output[i];
  }
}
```

## Encoding Process

The Skelly 64 VADPCM encoder uses the following process to encode VADPCM:

1. Break the input into frames of 16 samples each.

1. For each frame, calculate a 3x3 autocorrelation matrix.

1. Using a clustering algorithm, assign each frame to predictors. This algorithm searches for assignments that minimize the residual. The residual can be calculated from the autocorrelation matrix and the predictor coefficients, and the optimum predictor coefficients can be calculated from the autocorrelation matrix.

1. Calculate the predictor vectors from the optimum predictor coefficients for each predictor. This results in a codebook.

1. With the codebook and predictor assignments, encode each frame as VADPCM.

## Wire Format

Only AIFC files have a known way to carry VADPCM data. However, the codec is not explicitly tied to AIFC and could easily be adapted to other formats.

### Predictor Vectors

Each predictor is stored, one after the other. For example, if there are N predictors and the predictor order is K, the predictor vectors will have the following format:

| Type       | Description           |
| ---------- | --------------------- |
| `int16[8]` | Predictor 1, vector 1 |
| `int16[8]` | Predictor 1, vector 2 |
| …          | …                     |
| `int16[8]` | Predictor 1, vector K |
| `int16[8]` | Predictor 2, vector 1 |
| `int16[8]` | Predictor 2, vector 2 |
| …          | …                     |
| `int16[8]` | Predictor N, vector K |

### Common Chunk (AIFC)

The `'COMM'` chunk for VADPCM-encoded AIFC files uses a sample size of 16 bits.

The encoding type is `'VAPC'`, and the encoding name is `"VADPCM ~4-1"`.

### Codebook (AIFC)

The `VADPCMCODES` application-specific chunk in an AIFC file contains the codebook. The chunk, including its header, has the following format:

| Offset | Type       | Description                           |
| ------ | ---------- | ------------------------------------- |
| 0      | `uint32`   | Chunk ID, `'APPL'`                    |
| 4      | `uint32`   | Chunk size, starting after this field |
| 8      | `uint32`   | Application signature, `'stoc'`       |
| 12     | `uint8`    | Length of chunk name, 11              |
| 13     | `char[11]` | Chunk name, `"VADPCMCODES"`           |
| 24     | `uint16`   | VADPCM codec version, 1               |
| 26     | `uint16`   | Predictor order, 0-8                  |
| 28     | `uint16`   | Predictor count, 1-16                 |
| 30     | `int16[]`  | Predictor vectors                     |

### Audio Data

A frame of audio data is stored as 9 bytes.


| Offset | Type       | Description                           |
| ------ | ---------- | ------------------------------------- |
| 0      | `uint8`   | Control byte                    |
| 1      | `uint8[8]`   | Residual |

The control byte stores the scaling factor as the high four bits, and the predictor index in the low four bits.


Each byte in the residual encodes a pair of two successive residual audio values, stored as signed four-bit numbers. The first sample in each pair is stored in the high four bits and the second sample is stored in the low four bits.


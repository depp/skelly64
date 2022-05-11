// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/vadpcm/vadpcm.h"

#include <math.h>

// Autocorrelation is a symmetric 3x3 matrix.
//
// The upper triangle is stored. Indexes:
//
// [0 1 3]
// [_ 2 4]
// [_ _ 5]

// Calculate the square error, given an autocorrelation matrix and predictor
// coefficients.
static float vadpcm_eval(const float corr[restrict static 6],
                         const float coeff[restrict static 2]) {
    return corr[0] +                       //
           corr[2] * coeff[0] * coeff[0] + //
           corr[5] * coeff[1] * coeff[1] + //
           2.0f * (corr[1] * coeff[0] +    //
                   corr[3] * coeff[1] +    //
                   corr[4] * coeff[0] * coeff[1]);
}

// Calculate the predictor coefficients, given an autocorrelation matrix. The
// coefficients are chosen to minimize vadpcm_eval.
static void vadpcm_solve(const double corr[restrict static 6],
                         double coeff[restrict static 2]) {
    // For the autocorrelation matrix A, we want vector v which minimizes the
    // residual \epsilon,
    //
    // \epsilon = [1|v]^T A [1|v]
    //
    // We can rewrite this as:
    //
    // \epsilon = B + 2 C v + v^T D v
    //
    // Where B, C, and D are submatrixes of A. The minimum value, v, satisfies:
    //
    // D v + C = 0.

    double rel_epsilon = 1.0 / 4096.0;
    coeff[0] = 0.0;
    coeff[1] = 0.0;

    // The element with maximum absolute value is on the diagonal, by the
    // Cauchy-Schwarz inequality.
    double max = corr[0];
    if (corr[2] > max) {
        max = corr[2];
    }
    if (corr[5] > max) {
        max = corr[5];
    }
    double epsilon = max * rel_epsilon;

    // Solve using Gaussian elimination.
    //
    // [a b | x]
    // [b c | y]
    double a = corr[2];
    double b = corr[4];
    double c = corr[5];
    double x = -corr[1];
    double y = -corr[3];

    // Partial pivoting. Note that a, c are non-negative.
    int pivot = c > a;
    if (pivot) {
        double t;
        t = a;
        a = c;
        c = t;
        t = x;
        x = y;
        y = t;
    }

    // Multiply first row by 1/a: [1 b/a | x/a]
    if (a <= epsilon) {
        // Matrix is close to zero. Just use zero for the predictor
        // coefficients.
        return;
    }
    double a1 = 1.0 / a;
    double b1 = b * a1;
    double x1 = x * a1;

    // Subtract first row * b from second row: [0 c-b1*b | y - x1*b]
    double c2 = c - b1 * b;
    double y2 = y - x1 * b;

    // Multiply second row by 1/c. [0 1 | y2/c2]
    if (fabs(c2) <= epsilon) {
        // Matrix is poorly conditioned or singular. Solve as a first-order
        // system.
        coeff[pivot] = x1;
        return;
    }
    double y3 = y2 / c2;

    // Backsubstitute.
    double x4 = x1 - y3 * b1;

    coeff[pivot] = x4;
    coeff[!pivot] = y3;
}

#if TEST
#include <stdbool.h>
#include <stdio.h>

static bool did_fail;

static void test_solve(void) {
    // Check that vadpcm_solve minimizes vadpcm_eval.
    static const double dcorr[][6] = {
        // Simple positive definite matrixes.
        {4.0, 1.0, 5.0, 2.0, 3.0, 6.0},
        {4.0, -1.0, 5.0, -2.0, -3.0, 6.0},
        {4.0, 1.0, 6.0, 2.0, 3.0, 5.0},
        // Singular matrixes.
        {1.0, 0.5, 1.0, 0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0, 0.5, 0.0, 1.0},
        {1.0, 0.25, 2.0, 0.25, 2.0, 2.0},
        // Zero submatrix.
        {1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        // Zero.
        {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    };
    static const float offset[][2] = {
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {-1.0f, 0.0f},
        {0.0f, -1.0f},
    };
    static const float offset_amt = 0.01f;
    int failures = 0;
    for (size_t test = 0; test < sizeof(dcorr) / sizeof(*dcorr); test++) {
        double dcoeff[2];
        vadpcm_solve(dcorr[test], dcoeff);
        float corr[6];
        float coeff[2];
        for (int i = 0; i < 6; i++) {
            corr[i] = (float)dcorr[test][i];
        }
        for (int i = 0; i < 2; i++) {
            coeff[i] = (float)dcoeff[i];
        }
        // Test that this is a local minimum.
        float error = vadpcm_eval(corr, coeff);
        float min_error = error - error * (1.0f / 65536.0f);
        for (size_t off = 0; off < sizeof(offset) / sizeof(*offset); off++) {
            float ocoeff[2];
            for (int i = 0; i < 2; i++) {
                ocoeff[i] = coeff[i] + offset[off][i] * offset_amt;
            }
            float oerror = vadpcm_eval(corr, ocoeff);
            if (oerror < min_error) {
                fprintf(stderr, "test_solve case %zu: not a local minimum\n",
                        test);
                failures++;
            }
        }
    }
    if (failures > 0) {
        fprintf(stderr, "test_solve failures: %d\n", failures);
        did_fail = true;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    test_solve();
    return did_fail ? 1 : 0;
}

#endif // TEST

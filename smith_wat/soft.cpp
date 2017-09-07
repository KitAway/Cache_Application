#include <cstring>

/*
 * Copyright 2014, Xilinx Inc.
 * All rights reserved.
 */

/*
 * References:
 * https://github.com/l-urence/smith-waterman
 * http://amrita.vlab.co.in/?sub=3&brch=274&sim=1433&cnt=1
 * http://en.wikipedia.org/wiki/Smith%E2%80%93Waterman_algorithm
 */


void smithwaterman (int *matrix,
                    int *maxIndex,
                    const char *s1,
                    const char *s2,
                    const int N)
{
    short north = 0;
    short west = 0;
    short northwest = 0;
    const short GAP = -1;
    const short MATCH = 2;
    const short MISS_MATCH = -1;
    const short CENTER = 0;
    const short NORTH = 1;
    const short NORTH_WEST = 2;
    const short WEST = 3;
    int maxValue = 0;
    int localMaxIndex = 0;

    std::memset(matrix, 0, N * N * 4);

    for (short index = N; index < N * N; index++)
    {
        short dir = CENTER;
        short val = 0;
        short j = index % N;
        if (j == 0) { // Skip the first column
            west = 0;
            northwest = 0;
            continue;
        }
        short i = index / N;
        int temp = matrix[index - N];
        temp &= 0x0000ffff;
        north = (short)temp;
        const short match = (s1[j] == s2[i]) ? MATCH : MISS_MATCH;
        short val1 = northwest + match;

        if (val1 > val) {
            val = val1;
            dir = NORTH_WEST;
        }
        val1 = north + GAP;
        if (val1 > val) {
            val = val1;
            dir = NORTH;
        }
        val1 = west + GAP;
        if (val1 > val) {
            val = val1;
            dir = WEST;
        }
        temp = dir;
        temp <<= 16;
        temp |= val;
        matrix[index] = temp;
        west = val;
        northwest = north;
        if (val > maxValue) {
            localMaxIndex = index;
            maxValue = val;
        }
    }
    *maxIndex = localMaxIndex;
}


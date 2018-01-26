/**
 * Copyright (C) 2017 Alibaba Group Holding Limited. All Rights Reserved.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "VrEularAngleMatrix.h"

#include <multimedia/mm_debug.h>
#include <math.h>

//#include <WindowSurface.h>

namespace yunos {
namespace yvr {

#define PI (3.1415926f)

const char * VrEularAngleMatrix::MM_LOG_TAG = "VrTR";

// Transform matrices
static float mtxIdentity9[9] = {
    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
};

/* static */
void VrEularAngleMatrix::matrixMultiply(float result[9], const float l[9], const float r[9]) {
    if (result == l || result == r) {
        float out[9];

        out[0] = l[0]*r[0] + l[1]*r[3] + l[2]*r[6];
        out[1] = l[0]*r[1] + l[1]*r[4] + l[2]*r[7];
        out[2] = l[0]*r[2] + l[1]*r[5] + l[2]*r[8];

        out[3] = l[3]*r[0] + l[4]*r[3] + l[5]*r[6];
        out[4] = l[3]*r[1] + l[4]*r[4] + l[5]*r[7];
        out[5] = l[3]*r[2] + l[4]*r[5] + l[5]*r[8];

        out[6] = l[6]*r[0] + l[7]*r[3] + l[8]*r[6];
        out[7] = l[6]*r[1] + l[7]*r[4] + l[8]*r[7];
        out[8] = l[6]*r[2] + l[7]*r[5] + l[8]*r[8];

        memcpy(result, out, sizeof(out));
        return;
    }

    result[0] = l[0]*r[0] + l[1]*r[3] + l[2]*r[6];
    result[1] = l[0]*r[1] + l[1]*r[4] + l[2]*r[7];
    result[2] = l[0]*r[2] + l[1]*r[5] + l[2]*r[8];

    result[3] = l[3]*r[0] + l[4]*r[3] + l[5]*r[6];
    result[4] = l[3]*r[1] + l[4]*r[4] + l[5]*r[7];
    result[5] = l[3]*r[2] + l[4]*r[5] + l[5]*r[8];

    result[6] = l[6]*r[0] + l[7]*r[3] + l[8]*r[6];
    result[7] = l[6]*r[1] + l[7]*r[4] + l[8]*r[7];
    result[8] = l[6]*r[2] + l[7]*r[5] + l[8]*r[8];
}

/* static */
void VrEularAngleMatrix::getRotationDiffMatrix(
                    std::vector<float> &rd,
                    std::vector<float> R,
                    std::vector<float> prevR) {
    std::vector<float> &ri = R;
    std::vector<float> &pri = prevR;

    // calculate the parts of the rotation difference matrix we need
    // rd[i][j] = pri[0][i] * ri[0][j] + pri[1][i] * ri[1][j] + pri[2][i] * ri[2][j];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            rd[i * 3 + j] = pri[i] * ri[j] + pri[3+i] * ri[3+j] + pri[6+i] * ri[6+j];
}

VrEularAngleMatrix::VrEularAngleMatrix() {
    loadIdentity();
}

float* VrEularAngleMatrix::getMatrix() {
    return mMatrix;
}

void VrEularAngleMatrix::loadIdentity() {
    for (int i = 0; i < 9; i++)
        mMatrix[i] = mtxIdentity9[i];
}

/*
1 0     0
0 cos θ − sin θ
0 sin θ cos θ
*/
void VrEularAngleMatrix::applyXRotation(float radius) {
    float matrix[9];
    float result[9];

    matrix[0] = 1.0f;
    matrix[1] = 0.0f;
    matrix[2] = 0.0f;

    matrix[3] = 0.0f;
    matrix[4] = cosf(radius);
    matrix[5] = -sinf(radius);

    matrix[6] = 0.0f;
    matrix[7] = sinf(radius);
    matrix[8] = cosf(radius);

    VrEularAngleMatrix::matrixMultiply(result, matrix, mMatrix);
    memcpy(mMatrix, result, sizeof(result));
}

/*
cos θ   0 sin θ
0       1 0
− sin θ 0 cos θ
*/
void VrEularAngleMatrix::applyYRotation(float radius) {
    float matrix[9];
    float result[9];

    matrix[0] = cosf(radius);
    matrix[1] = 0.0f;
    matrix[2] = sinf(radius);

    matrix[3] = 0.0f;
    matrix[4] = 1.0f;
    matrix[5] = 0.0f;

    matrix[6] = -sinf(radius);
    matrix[7] = 0.0f;
    matrix[8] = cosf(radius);

    VrEularAngleMatrix::matrixMultiply(result, matrix, mMatrix);
    memcpy(mMatrix, result, sizeof(result));
}

void applyZRotation(float radius) {

}

/* static */
float VrEularAngleMatrix::length(float x, float y, float z) {
    return (float) sqrt(x * x + y * y + z * z);
}

} // end of namespace yvr
} // end of namespace yunos

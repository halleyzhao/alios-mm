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

#include "VrTransformMatrix.h"

#include <multimedia/mm_debug.h>

//#include <WindowSurface.h>

namespace yunos {
namespace yvr {

#define PI (3.1415926f)

const char * VrTransformMatrix::MM_LOG_TAG = "VrTR";

// Transform matrices
static float mtxIdentity[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
};
static float mtxFlipH[16] = {
    -1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    1, 0, 0, 1,
};
static float mtxFlipV[16] = {
    1, 0, 0, 0,
    0, -1, 0, 0,
    0, 0, 1, 0,
    0, 1, 0, 1,
};
static float mtxRot90[16] = {
    0, 1, 0, 0,
    -1, 0, 0, 0,
    0, 0, 1, 0,
    1, 0, 0, 1,
};

/* static */
void VrTransformMatrix::matrixMultiply(float result[16], const float x[16], const float y[16]) {
    result[0] = x[0]*y[0] + x[4]*y[1] + x[8]*y[2] + x[12]*y[3];
    result[1] = x[1]*y[0] + x[5]*y[1] + x[9]*y[2] + x[13]*y[3];
    result[2] = x[2]*y[0] + x[6]*y[1] + x[10]*y[2] + x[14]*y[3];
    result[3] = x[3]*y[0] + x[7]*y[1] + x[11]*y[2] + x[15]*y[3];

    result[4] = x[0]*y[4] + x[4]*y[5] + x[8]*y[6] + x[12]*y[7];
    result[5] = x[1]*y[4] + x[5]*y[5] + x[9]*y[6] + x[13]*y[7];
    result[6] = x[2]*y[4] + x[6]*y[5] + x[10]*y[6] + x[14]*y[7];
    result[7] = x[3]*y[4] + x[7]*y[5] + x[11]*y[6] + x[15]*y[7];

    result[8] = x[0]*y[8] + x[4]*y[9] + x[8]*y[10] + x[12]*y[11];
    result[9] = x[1]*y[8] + x[5]*y[9] + x[9]*y[10] + x[13]*y[11];
    result[10] = x[2]*y[8] + x[6]*y[9] + x[10]*y[10] + x[14]*y[11];
    result[11] = x[3]*y[8] + x[7]*y[9] + x[11]*y[10] + x[15]*y[11];

    result[12] = x[0]*y[12] + x[4]*y[13] + x[8]*y[14] + x[12]*y[15];
    result[13] = x[1]*y[12] + x[5]*y[13] + x[9]*y[14] + x[13]*y[15];
    result[14] = x[2]*y[12] + x[6]*y[13] + x[10]*y[14] + x[14]*y[15];
    result[15] = x[3]*y[12] + x[7]*y[13] + x[11]*y[14] + x[15]*y[15];
}

VrTransformMatrix::VrTransformMatrix()
    : mX(0),
      mY(0),
      mWidth(0),
      mHeight(0),
      mBufWidth(-1),
      mBufHeight(-1),
      mStride(-1),
      mFormat(-1),
      mUsage(-1),
      mTransform(-1) {

    mCrop[0] = 0;
    mCrop[1] = 0;
    mCrop[2] = 0;
    mCrop[3] = 0;

    mPitch = 0.f;
    mRoll = 0.f;
    mYaw = 0.f;

    loadIdentity();
}

VrTransformMatrix::VrTransformMatrix(int crop[])
    : mX(crop[0]),
      mY(crop[1]),
      mWidth(crop[2]),
      mHeight(crop[3]),
      mBufWidth(-1),
      mBufHeight(-1),
      mStride(-1),
      mFormat(-1),
      mUsage(-1),
      mTransform(-1) {

    mCrop[0] = 0;
    mCrop[1] = 0;
    mCrop[2] = 0;
    mCrop[3] = 0;

    mPitch = 0.f;
    mRoll = 0.f;
    mYaw = 0.f;

    loadIdentity();
}

void VrTransformMatrix::calculateTexMatrix(MMNativeBuffer* anb, float pose[]) {

    float result[16];
    int crop[4]; // x y w h

    mBufWidth = anb->width;
    mBufHeight = anb->height;
    mStride = anb->stride;
    mFormat = anb->format;
    //mUsage = anb->usage;

    mPitch = pose[0];
    mRoll = pose[1];
    mYaw = pose[2];

    for (int i = 0; i < 16; i++)
        mMatrix[i] = mtxIdentity[i];

    int transform = 0;

    if (mTransform == 90)
        transform = NATIVE_WINDOW_TRANSFORM_ROT_90;
    else if (mTransform == 180)
        transform = NATIVE_WINDOW_TRANSFORM_ROT_180;
    else if (mTransform == 270)
        transform = NATIVE_WINDOW_TRANSFORM_ROT_270;

    if (transform & NATIVE_WINDOW_TRANSFORM_FLIP_H) {
        matrixMultiply(result, mMatrix, mtxFlipH);
        for (int i = 0; i < 16; i++) {
            mMatrix[i] = result[i];
        }
    }
    if (transform & NATIVE_WINDOW_TRANSFORM_FLIP_V) {
        matrixMultiply(result, mMatrix, mtxFlipV);
        for (int i = 0; i < 16; i++) {
            mMatrix[i] = result[i];
        }
    }
    if (transform & NATIVE_WINDOW_TRANSFORM_ROT_90) {
        matrixMultiply(result, mMatrix, mtxRot90);
        for (int i = 0; i < 16; i++) {
            mMatrix[i] = result[i];
        }
    }

    calculateCrop(crop);

    if (mCrop[0] != crop[0] || mCrop[1] != crop[1] ||
        mCrop[2] != crop[2] || mCrop[3] != crop[3]) {

        INFO("crop %d %d %d %d in buffer %dx%d",
                crop[0], crop[1], crop[2], crop[3],
                mBufWidth, mBufHeight);
    }

    if ((crop[0] < 0 || crop[0] >= mBufWidth) ||
        (crop[1] < 0 || crop[1] >= mBufHeight) ||
        (crop[2] <= 0 || (crop[2] + crop[0]) > mBufWidth) ||
        (crop[3] <= 0 || (crop[3] + crop[1]) > mBufHeight)) {
        WARNING("invalid buffer crop");
        goto out;
    }

    mCrop[0] = crop[0];
    mCrop[1] = crop[1];
    mCrop[2] = crop[2];
    mCrop[3] = crop[3];

    if (crop[0] || crop[1] || crop[2] < mBufWidth || crop[3] < mBufHeight) {
        float tx = 0.0f, ty = 0.0f, x = 1.0f, y = 1.0f;
        float bufferWidth = mBufWidth;
        float bufferHeight = mBufHeight;
        float scale = 1.0f;
        if (1) {
            switch (mFormat) {
                case 'none':
                    scale = 0.5;
                    break;

                default:
                    scale = 1.0;
                    break;
            }
        }

        // Only shrink the dimensions that are not the size of the buffer.
        if (crop[2] < bufferWidth) {
            tx = (float(crop[0]) + scale) / bufferWidth;
            x = (float(crop[2]) - (2.0f * scale)) /
                    bufferWidth;
        }

        if (crop[3] < bufferHeight) {
            ty = (float(bufferHeight - (crop[1] + crop[3])) + scale) /
                    bufferHeight;
            y = (float(crop[3]) - (2.0f * scale)) /
                    bufferHeight;
        }

        float cropMatrix[16] = {
            x, 0, 0, 0,
            0, y, 0, 0,
            0, 0, 1, 0,
            tx, ty, 0, 1,
        };

        matrixMultiply(result, cropMatrix, mMatrix);
        for (int i = 0; i < 16; i++) {
            mMatrix[i] = result[i];
        }
    }

out:
    matrixMultiply(result, mMatrix, mtxFlipV);
    for (int i = 0; i < 16; i++)
        mMatrix[i] = result[i];

}

GLfloat* VrTransformMatrix::getMatrix() {
    return mMatrix;
}

void VrTransformMatrix::calculateCrop(int crop[]) {

    float fovLeft = PI/4;
    float fovRight = PI/4;
    float fovUp = PI/4;
    float fovDown = PI/4;

    int displayHeight;
    int displayWidth;

    float pitch = mPitch;
    float roll = mRoll;
    float yaw = mYaw;

    if (pitch != 0 || roll != 0) {
        WARNING("pitch and roll are not supported");
        pitch = 0.f;
        roll = 0.f;
    }

    /* MONO content with single view */
    displayHeight = mBufHeight * (fovUp + fovDown) / PI;
    displayWidth = displayHeight * (fovLeft + fovRight) / (fovUp + fovDown) * mWidth / mHeight;

    crop[0] = (mBufWidth - displayWidth) / 2 - mBufWidth * yaw / (2 * PI);
    if (crop[0] < 0)
        crop[0] = 0;
    else if (crop[0] > mBufWidth - displayWidth)
        crop[0] = mBufWidth - displayWidth;

    crop[1] = (mBufHeight - displayHeight) / 2 - mBufHeight * pitch / PI;
    if (crop[1] < 0)
        crop[1] = 0;
    else if (crop[1] > mBufHeight - displayHeight)
        crop[1] = mBufHeight - displayHeight;

    crop[2] = displayWidth;
    crop[3] = displayHeight;
}

void VrTransformMatrix::loadIdentity() {
    for (int i = 0; i < 16; i++)
        mMatrix[i] = mtxIdentity[i];
}

/* static */
float VrTransformMatrix::length(float x, float y, float z) {
    return (float) sqrt(x * x + y * y + z * z);
}

void VrTransformMatrix::setLookAtMatrix(float eyeX, float eyeY, float eyeZ,
                                              float centerX, float centerY, float centerZ,
                                              float upX, float upY, float upZ) {
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;

    float l = 1.0f / length(fx, fy, fz);
    fx *= l;
    fy *= l;
    fz *= l;

    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;

    l = 1.0f / length(sx, sy, sz);
    sx *= l;
    sy *= l;
    sz *= l;

    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    mMatrix[8] = sz;
    mMatrix[9] = uz;
    mMatrix[10] = -fz;
    mMatrix[11] = 0.0f;

    mMatrix[12] = 0.0f;
    mMatrix[13] = 0.0f;
    mMatrix[14] = 0.0f;
    mMatrix[15] = 1.0f;

    mMatrix[0] = sx;
    mMatrix[1] = ux;
    mMatrix[2] = -fx;
    mMatrix[3] = 0.0f;

    mMatrix[4] = sy;
    mMatrix[5] = uy;
    mMatrix[6] = -fy;
    mMatrix[7] = 0.0f;

    translateM(-eyeX, -eyeY, -eyeZ);
}

void VrTransformMatrix::translateM(float x, float y, float z) {
    for (int i=0 ; i<4 ; i++) {
        int m = i;
        mMatrix[12 + m] += mMatrix[m] * x + mMatrix[4 + m] * y + mMatrix[8 + m] * z;
    }
}

void VrTransformMatrix::setFrustumMatrix(float l, float r,
                          float b, float t,
                          float n, float f) {

    float width  = 1.0f / (r - l);
    float height = 1.0f / (t - b);
    float depth  = 1.0f / (n - f);
    float ux = 2.0f * (n * width);
    float uy = 2.0f * (n * height);

    float a1 = (r + l) * width;
    float a2 = (t + b) * height;
    float a3 = (f + n) * depth;
    float a4 = 2.0f * (f * n * depth);

    mMatrix[0] = ux;

    mMatrix[1] = 0.0f;
    mMatrix[2] = 0.0f;
    mMatrix[3] = 0.0f;
    mMatrix[4] = 0.0f;
    mMatrix[5] = uy;
    mMatrix[6] = 0.0f;
    mMatrix[7] = 0.0f;
    mMatrix[8] = a1;
    mMatrix[9] = a2;
    mMatrix[10] = a3;
    mMatrix[11] = -1.0f;
    mMatrix[12] = 0.0f;
    mMatrix[13] = 0.0f;
    mMatrix[14] = a4;
    mMatrix[15] = 0.0f;
}

} // end of namespace yvr
} // end of namespace yunos

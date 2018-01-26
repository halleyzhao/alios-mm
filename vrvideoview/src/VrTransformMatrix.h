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

#ifndef __vr_matrix_transform_h
#define __vr_matrix_transform_h

#include <GLES2/gl2.h>
#include <native_surface_help.h>

namespace yunos {
namespace yvr {

class VrTransformMatrix {
public:
    VrTransformMatrix(int crop[]);
    VrTransformMatrix();
    ~VrTransformMatrix() { }

    void calculateTexMatrix(MMNativeBuffer* anb, float pose[]);
    GLfloat* getMatrix();

    void setTransform(int transform) { mTransform = transform; }
    void loadIdentity();

    void setLookAtMatrix(float eyeX, float eyeY, float eyeZ,
                               float centerX, float centerY, float centerZ,
                               float upX, float upY, float upZ);

    void setFrustumMatrix(float left, float right,
                          float bottom, float top,
                          float near, float far);

    void translateM(float x, float y, float z);
    static void matrixMultiply(float result[16], const float x[16], const float y[16]);

private:
    void calculateCrop(int crop[]);
    static float length(float x, float y, float z);

    // view port
    int32_t mX;
    int32_t mY;
    int32_t mWidth;
    int32_t mHeight;

    // transform matrix
    GLfloat mMatrix[16];

    // src buffer
    int mBufWidth;
    int mBufHeight;
    int mStride;
    int mFormat;
    int mUsage;
    int mTransform;

    // src buffer crop
    int mCrop[4];

    // head pose
    float mPitch;
    float mRoll;
    float mYaw;

private:
    static const char * MM_LOG_TAG;

    VrTransformMatrix(const VrTransformMatrix&);
    VrTransformMatrix& operator=(const VrTransformMatrix&);
};

} // end of namespace yvr
} // end of namespace yunos
#endif // __vr_matrix_transform_h

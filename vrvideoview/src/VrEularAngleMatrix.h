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

#ifndef __vr_eular_angle_matrix_h
#define __vr_eular_angle_matrix_h

#include <vector>

namespace yunos {
namespace yvr {

class VrEularAngleMatrix {
public:
    VrEularAngleMatrix();
    ~VrEularAngleMatrix() { }

    float* getMatrix();

    void loadIdentity();

    void applyXRotation(float); // pitch
    void applyYRotation(float); // yaw
    void applyZRotation(float); // roll

    static void matrixMultiply(float result[9], const float l[9], const float r[9]);
    static void getRotationDiffMatrix(
                    std::vector<float> &rd,
                    std::vector<float> R,
                    std::vector<float> prevR);

private:
    static float length(float x, float y, float z);

    // transform matrix
    float mMatrix[9];

private:
    static const char * MM_LOG_TAG;

    VrEularAngleMatrix(const VrEularAngleMatrix&);
    VrEularAngleMatrix& operator=(const VrEularAngleMatrix&);
};

} // end of namespace yvr
} // end of namespace yunos
#endif // __vr_eular_angle_matrix_h

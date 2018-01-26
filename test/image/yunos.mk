#############################################################################
# Copyright (C) 2015-2017 Alibaba Group Holding Limited. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#############################################################################


LOCAL_PATH:=$(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../base/build/build.mk

LOCAL_SRC_FILES:= bmp_test.cc

LOCAL_C_INCLUDES:= \
    $(MM_INCLUDE) \
    $(base-includes) \
    $(multimedia-image-includes)

LOCAL_SHARED_LIBRARIES += libmmbase libimage

LOCAL_MODULE:= bmp-test
LOCAL_LDFLAGS += -lstdc++
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../base/build/build.mk

LOCAL_SRC_FILES:= wbmp_test.cc

LOCAL_C_INCLUDES:= \
    $(MM_INCLUDE) \
    $(base-includes) \
    $(multimedia-image-includes)

LOCAL_SHARED_LIBRARIES += libmmbase libimage

LOCAL_MODULE:= wbmp-test
LOCAL_LDFLAGS += -lstdc++
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../base/build/build.mk

LOCAL_SRC_FILES:= regiondecoder_test.cc

LOCAL_C_INCLUDES:= \
    $(MM_INCLUDE) \
    $(base-includes) \
    $(multimedia-image-includes)

LOCAL_SHARED_LIBRARIES += libmmbase libimage

LOCAL_MODULE:= regiondecoder-test
LOCAL_LDFLAGS += -lstdc++
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../base/build/build.mk

LOCAL_SRC_FILES:= image_test.cc

LOCAL_C_INCLUDES:= \
    $(MM_INCLUDE) \
    $(base-includes) \
    $(multimedia-image-includes)

include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SHARED_LIBRARIES += libmmbase libimage
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_MODULE:= image-test
LOCAL_LDFLAGS += -lstdc++
include $(BUILD_EXECUTABLE)





include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../base/build/build.mk

LOCAL_SRC_FILES:= exif_test.cc

LOCAL_C_INCLUDES:= \
    $(MM_INCLUDE) \
    $(base-includes) \
    $(multimedia-image-includes)

include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SHARED_LIBRARIES += libmmbase libimage
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_MODULE:= exif-test
LOCAL_LDFLAGS += -lstdc++
include $(BUILD_EXECUTABLE)


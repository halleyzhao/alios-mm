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
MM_ROOT_PATH:= $(LOCAL_PATH)/../../../

ifneq ($(XMAKE_PLATFORM),ivi)
#### mm-pagewindow-test
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/cow/build/cow_common.mk
LOCAL_SRC_FILES := pagewindow_test.cc \
                   $(MM_EXT_PATH)/WindowSurface/SimpleSubWindow.cc

LOCAL_C_INCLUDES += \
    $(WINDOWSURFACE_INCLUDE)     \
    $(HYBRIS_EGLPLATFORMCOMMON)  \
    $(MM_EXT_PATH)/WindowSurface \
    $(pagewindow-includes)       \
    $(libuv-includes)


LOCAL_CPPFLAGS += -DMP_HYBRIS_EGLPLATFORM="\"wayland\"" -fpermissive
LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof $(WINDOWSURFACE_CPPFLAGS)
LOCAL_LDFLAGS += -lpthread -ldl -lstdc++

REQUIRE_PAGEWINDOW = 1
REQUIRE_WAYLAND = 1
REQUIRE_EGL = 1
REQUIRE_SURFACE = 1
REQUIRE_WPC = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_MODULE := mm-pagewindow-test

include $(BUILD_EXECUTABLE)

endif

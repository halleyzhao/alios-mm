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
MM_ROOT_PATH:= $(LOCAL_PATH)/../../

ifneq ($(XMAKE_PLATFORM),ivi)
#### attach-buffer-test
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_C_INCLUDES += \
    $(base-includes)            \
    $(graphics-includes)         \
    $(libuv-includes)            \
    $(MM_ROOT_PATH)/base/include \
    $(MM_HELP_WINDOWSURFACE_INCLUDE)

LOCAL_LDFLAGS += -lpthread  -lstdc++
LOCAL_SHARED_LIBRARIES += libmmbase
LOCAL_CFLAGS_REMOVED := -march=armv7-a

ifeq ($(XMAKE_ENABLE_CNTR_HAL),true)
    LOCAL_C_INCLUDES += \
        $(HYBRIS_EGLPLATFORMCOMMON)
    LOCAL_CPPFLAGS += -DMP_HYBRIS_EGLPLATFORM="\"wayland\"" -fpermissive
    LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof $(WINDOWSURFACE_CPPFLAGS)
endif


## surface related dep
REQUIRE_WPC = 1
REQUIRE_WAYLAND = 1
REQUIRE_EGL = 1
REQUIRE_SURFACE = 1
ifeq ($(XMAKE_ENABLE_CNTR_HAL),true)
    REQUIRE_LIBHYBRIS = 1
endif
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_SRC_FILES := attach_buffer_test.cc \
                   $(MM_EXT_PATH)/WindowSurface/WindowSurfaceTestWindow.cc

LOCAL_MODULE := attach-buffer-test
include $(BUILD_EXECUTABLE)
endif

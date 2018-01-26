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

ifeq ($(XMAKE_ENABLE_CNTR_HAL),true)

LOCAL_PATH:=$(call my-dir)
MM_ROOT_PATH:= $(LOCAL_PATH)/../../

#### vrplayer-test
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

include $(MM_ROOT_PATH)/cow/build/cow_common.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

ifeq ($(USING_VR_VIDEO),1)

LOCAL_SRC_FILES := vrplayer_test.cc                                        \
                   $(MM_EXT_PATH)/WindowSurface/WindowSurfaceTestWindow.cc \
                   $(MM_EXT_PATH)/native_surface_help.cc                   \
                   $(MM_EXT_PATH)/WindowSurface/SimpleSubWindow.cc

LOCAL_C_INCLUDES += \
    $(MM_MEDIACODEC_INCLUDE)     \
    $(LIBAV_INCLUDE)             \
    $(MM_HELP_WINDOWSURFACE_INCLUDE)    \
    $(libhybris-includes)        \
    $(MM_ROOT_PATH)/mediaplayer/include \
    $(vr-video-includes)         \
    $(pagewindow-includes)       \
    $(libuv-includes)            \
    $(MM_WAKELOCKER_PATH)        \
    $(power-includes)            \
    $(multimedia-surfacetexture-includes)

LOCAL_CPPFLAGS += -DMP_HYBRIS_EGLPLATFORM="\"wayland\"" -fpermissive
LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof $(WINDOWSURFACE_CPPFLAGS)
LOCAL_CPPFLAGS += -DMM_ENABLE_PAGEWINDOW

LOCAL_LDFLAGS += -lpthread -lstdc++

LOCAL_SHARED_LIBRARIES += libmediaplayer
LOCAL_SHARED_LIBRARIES += libmediasurfacetexture

LOCAL_SHARED_LIBRARIES += libyvr_view

REQUIRE_LIBHYBRIS = 1
REQUIRE_WAYLAND = 1
REQUIRE_SURFACE =1
REQUIRE_WPC = 1
REQUIRE_PAGEWINDOW = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk


LOCAL_MODULE := vrplayer-test

include $(BUILD_EXECUTABLE)

endif
endif

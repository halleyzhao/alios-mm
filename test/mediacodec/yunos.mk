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

##### libmediacodec_play
include $(CLEAR_VARS)
include  $(LOCAL_PATH)/mediacodec_common.mk

LOCAL_SRC_FILES:= \
    $(MM_EXT_PATH)/egl/egl_texture.cc \
    mediacodecPlay.cc

## LOCAL_SRC_FILE += $(MM_HELP_WINDOWSURFACE_SRC_FILES)
LOCAL_SRC_FILES += $(MM_EXT_PATH)/WindowSurface/WindowSurfaceTestWindow.cc \
                   $(MM_EXT_PATH)/WindowSurface/SimpleSubWindow.cc         \
                   $(MM_EXT_PATH)/native_surface_help.cc

LOCAL_C_INCLUDES += $(MM_HELP_WINDOWSURFACE_INCLUDE)    \
    $(MM_MEDIACODEC_INCLUDE) \
    $(libhybris-includes)    \
    $(libuv-includes)        \
    $(libav-includes)        \
    $(pagewindow-includes)   \
    $(multimedia-surfacetexture-includes)
LOCAL_C_INCLUDES += $(MM_MEDIACODEC_NOTUSE_COMPAT_INCLUDE)
LOCAL_CPPFLAGS += -DMM_ENABLE_PAGEWINDOW

REQUIRE_PAGEWINDOW = 1
REQUIRE_EGL = 1
REQUIRE_LIBAV = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk
LOCAL_SHARED_LIBRARIES += libmediacodec_yunos libmediasurfacetexture

LOCAL_LDFLAGS += -lpthread -lstdc++

LOCAL_MODULE:= libmediacodec_play
include $(BUILD_SHARED_LIBRARY)

#####mediacodec-test
include $(CLEAR_VARS)
include $(LOCAL_PATH)/mediacodec_common.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SRC_FILES:= mediacodectest.cc

LOCAL_C_INCLUDES += \
    $(MM_MEDIACODEC_INCLUDE)          \
    $(MM_HELP_WINDOWSURFACE_INCLUDE)  \
    $(MM_WAKELOCKER_PATH)             \
    $(power-includes)

# LOCAL_C_INCLUDES += $(MM_MEDIACODEC_NOTUSE_COMPAT_INCLUDE)

LOCAL_LDFLAGS += -lpthread -lstdc++
LOCAL_CPPFLAGS += -std=c++11

LOCAL_SHARED_LIBRARIES += libmediacodec_play libmmwakelocker

LOCAL_MODULE:= mediacodec-test
include $(BUILD_EXECUTABLE)

##### dbusplayer-example-service
include $(CLEAR_VARS)

COMIPLE_SURFACE_TEXTURE := 0
include $(LOCAL_PATH)/mediacodec_common.mk

LOCAL_SRC_FILES:= DBusPlayerService.cc

LOCAL_C_INCLUDES += \
    $(dbus-includes)    \
    $(libuv-includes)

LOCAL_LDFLAGS += -lpthread -lstdc++
LOCAL_CPPFLAGS += -std=c++11

LOCAL_SHARED_LIBRARIES += libmediacodec_play

REQUIRE_LIBUV = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_MODULE:= dbusplayer-example-service
include $(BUILD_EXECUTABLE)

##### dbusplayer-example-client
include $(CLEAR_VARS)
include $(LOCAL_PATH)/mediacodec_common.mk

LOCAL_SRC_FILES:= DBusPlayerClient.cc

LOCAL_C_INCLUDES += \
    $(dbus-includes)    \
    $(libuv-includes)

LOCAL_SHARED_LIBRARIES += libmediacodec_play

REQUIRE_LIBUV = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_MODULE:= dbusplayer-example-client
include $(BUILD_EXECUTABLE)

ifeq ($(filter $(XMAKE_BOARD), qcom qemu mstar sprd), )
##### camera-source-test
include $(CLEAR_VARS)
include $(LOCAL_PATH)/mediacodec_common.mk

LOCAL_SRC_FILES:= cameraSource.cc

LOCAL_C_INCLUDES += \
    $(MM_MEDIACODEC_INCLUDE)

LOCAL_C_INCLUDES += MM_MEDIACODEC_NOTUSE_COMPAT_INCLUDE

LOCAL_LDFLAGS +=  -lstdc++ -lpthread

REQUIRE_COMPAT = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_MODULE:= camera-source-test
include $(BUILD_EXECUTABLE)
endif ## ifeq ($(filter $(XMAKE_BOARD), qcom qemu))

##### mediacodec-record-test
include $(CLEAR_VARS)
include $(LOCAL_PATH)/mediacodec_common.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SRC_FILES:= mediacodecRecordTest.cc
LOCAL_SRC_FILES += $(MM_EXT_PATH)/WindowSurface/WindowSurfaceTestWindow.cc \
                   $(MM_EXT_PATH)/native_surface_help.cc

LOCAL_C_INCLUDES += $(MM_HELP_WINDOWSURFACE_INCLUDE) \
    $(MM_MEDIACODEC_INCLUDE)  \
    $(libav-includes)         \
    $(MM_WAKELOCKER_PATH)     \
    $(power-includes)
LOCAL_C_INCLUDES += $(MM_MEDIACODEC_NOTUSE_COMPAT_INCLUDE)
LOCAL_CPPFLAGS += -std=c++11

LOCAL_LDFLAGS += -lpthread -lstdc++
LOCAL_SHARED_LIBRARIES += libmediacodec_yunos libcowbase

LOCAL_SHARED_LIBRARIES += libmmwakelocker

REQUIRE_LIBAV = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_MODULE:= mediacodec-record-test
include $(BUILD_EXECUTABLE)

endif

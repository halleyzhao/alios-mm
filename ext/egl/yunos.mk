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

ifeq ($(USING_DRM_SURFACE),1)
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/cow/build/cow_common.mk

LOCAL_C_INCLUDES += \
    $(MM_ROOT_PATH)/mediaplayer/include \
    $(XMAKE_BUILD_MODULE_OUT)/rootfs/usr/include \
    $(libav-includes)                   \
    $(graphics-includes)                 \
    $(multimedia-surfacetexture-includes) \
    $(pagewindow-includes)       \
    $(libuv-includes)            \
    $(MM_WAKELOCKER_PATH)        \
    $(MM_HELP_WINDOWSURFACE_INCLUDE)

LIBGBM-INCLUDES:=   $(YUNOS_ROOT)/third_party/libgbm                       \

LOCAL_C_INCLUDES += ../../../ext/mm_amhelper/include/
LOCAL_C_INCLUDES += $(libdrm-includes)
LOCAL_C_INCLUDES += ${LIBGBM-INCLUDES}
LOCAL_C_INCLUDES += ${TI-LIBDCE-INCLUDES}

LOCAL_LDFLAGS += -lpthread  -lstdc++

REQUIRE_WPC = 1
REQUIRE_WAYLAND = 1
REQUIRE_EGL = 1
REQUIRE_SURFACE = 1
REQUIRE_LIBDCE = 1
REQUIRE_LIBDRM = 1
REQUIRE_LIBMMRPC = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk


LOCAL_SRC_FILES := egl_sanity_check.cc
LOCAL_MODULE := egl-sanity-check
include $(BUILD_EXECUTABLE)
endif


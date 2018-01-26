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

LOCAL_PATH:=  $(call my-dir)
MM_ROOT_PATH:= $(LOCAL_PATH)/../../

##libyvr_view.so
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
include $(MM_ROOT_PATH)/cow/build/cow_common.mk

###note first include build.mk
ifeq ($(USING_VR_VIDEO),1)

LOCAL_C_INCLUDES +=                           \
     $(LOCAL_PATH)/../include                \
     $(MM_INCLUDE)                           \
     $(base-includes)                        \
     $(WINDOWSURFACE_INCLUDE)                \
     $(MM_MEDIACODEC_INCLUDE)                \
     $(MM_ROOT_PATH)/ext/WindowSurface       \
     $(MM_ROOT_PATH)/ext/                    \
     $(libhybris-includes)                   \
     $(libuv-includes)                       \
     $(MM_WAKELOCKER_PATH)                   \
     $(power-includes)                        \
     $(sensormanager-includes)               \
     $(MM_ROOT_PATH)/cow/include/            \
     $(MM_ROOT_PATH)/ext/WindowSurface       \
     $(multimedia-surfacetexture-includes)

LOCAL_SRC_FILES:= VrVideoView.cc              \
                  VrVideoViewImpl.cc          \
                  VrViewController.cc         \
                  VrViewControllerImpl.cc     \
                  VrViewEglContext.cc         \
                  VrTransformMatrix.cc        \
                  VrEularAngleMatrix.cc       \
                  $(MM_ROOT_PATH)/ext/WindowSurface/WindowSurfaceTestWindow.cc \
                  $(MM_EXT_PATH)/native_surface_help.cc

LOCAL_MODULE := libyvr_view

LOCAL_SHARED_LIBRARIES += libmmbase libmediasurfacetexture libsensor

LOCAL_LDFLAGS += -lstdc++ -lpthread

LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof $(WINDOWSURFACE_CPPFLAGS) -DUSE_GLES_V2

REQUIRE_LIBHYBRIS = 1
REQUIRE_WAYLAND = 1
REQUIRE_SURFACE =1
REQUIRE_WPC = 1
REQUIRE_EGL = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)
endif
endif

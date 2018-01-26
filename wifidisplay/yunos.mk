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


LOCAL_PATH:=  $(call my-dir)
MM_ROOT_PATH:= $(LOCAL_PATH)/../

##libwifi_display.so
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/cow/build/cow_common.mk
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_C_INCLUDES:=             \
     $(LOCAL_PATH)/include     \
     $(MM_INCLUDE)             \
     $(base-includes)          \
     $(MM_HELP_WINDOWSURFACE_INCLUDE)   \
     $(HYBRIS_EGLPLATFORMCOMMON)  \
     $(MM_MEDIACODEC_INCLUDE)     \
     $(audioserver-includes)      \
     $(libav-includes)            \
     $(libhybris-includes)               \
     $(MM_ROOT_PATH)/mediaplayer/include \
     $(pagewindow-includes)       \
     $(libuv-includes)            \
     $(MM_WAKELOCKER_PATH)        \
     $(power-includes)            \
     $(WINDOWSURFACE_INCLUDE)     \
     $(cameraserver-includes)     \
     $(corefoundation-includes)   \
     $(MM_ROOT_PATH)/cow/include/      \
     $(MM_ROOT_PATH)/mediaplayer/include/      \
     $(MM_ROOT_PATH)/mediarecorder/include/

LOCAL_SRC_FILES:= src/WfdRtspFramework.cc        \
                  src/WfdSession.cc              \
                  src/WfdSourceSession.cc        \
                  src/WfdParameters.cc           \
                  src/WfdSinkSession.cc          \
                  $(MM_ROOT_PATH)/ext/WindowSurface/WindowSurfaceTestWindow.cc \
                  $(MM_ROOT_PATH)/ext/native_surface_help.cc \

LOCAL_MODULE := libwifi_display

LOCAL_SHARED_LIBRARIES += libmmbase libcowrecorder libcowplayer

LOCAL_CPPFLAGS += -DMP_HYBRIS_EGLPLATFORM="\"wayland\"" -fpermissive
LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof $(WINDOWSURFACE_CPPFLAGS)
LOCAL_LDFLAGS += -lpthread -ldl -lstdc++

REQUIRE_WAYLAND = 1
REQUIRE_SURFACE = 1
REQUIRE_WPC = 1
ifneq ($(XMAKE_PLATFORM),ivi)
REQUIRE_CAMERASVR = 1
endif
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)

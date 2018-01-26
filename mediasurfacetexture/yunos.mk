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

LOCAL_PATH:= $(call my-dir)
MM_ROOT_PATH:= $(LOCAL_PATH)/../

#### wayland drm surface
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_SRC_FILES:= $(MM_ROOT_PATH)/cow/src/platform/ivi/wayland-drm-protocol.c \
                  src/mm_wayland_drm_surface.cc

LOCAL_C_INCLUDES+=  $(wayland-includes)                      \
                    $(multimedia-base-includes)              \
                    $(libdrm-includes)                       \
                    ${TI-LIBDCE-INCLUDES}                       \
                    $(LOCAL_PATH)/include                    \
                    ${WINDOWSURFACE_INCLUDE}                 \
                    $(MM_ROOT_PATH)/cow/src/platform/ivi/

LOCAL_SHARED_LIBRARIES += libmmbase
LOCAL_LDFLAGS += -lstdc++ -lpthread

REQUIRE_WAYLAND = 1
REQUIRE_SURFACE = 1
REQUIRE_LIBDCE = 1
REQUIRE_LIBDRM = 1
REQUIRE_LIBMMRPC = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk


LOCAL_MODULE:= libWlDrmSurface
ifeq ($(USING_DRM_SURFACE),1)
include $(BUILD_SHARED_LIBRARY)
endif

#### vpu surface
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_SRC_FILES:= src/mm_vpu_surface.cc

LOCAL_C_INCLUDES+=  $(wayland-includes)                      \
                    $(multimedia-base-includes)              \
                    $(libdrm-includes)                       \
                    ${TI-LIBDCE-INCLUDES}                       \
                    $(LOCAL_PATH)/include                    \
                    ${WINDOWSURFACE_INCLUDE}                 \
                    $(MM_ROOT_PATH)/cow/src/platform/ivi/    \
                    $(MM_INCLUDE)

LOCAL_SHARED_LIBRARIES += libmmbase
LOCAL_LDFLAGS += -lstdc++ -lpthread


LOCAL_MODULE:= libvpuSurface
ifeq ($(USING_VPU_SURFACE),1)
include $(BUILD_SHARED_LIBRARY)
endif

####libmediasurfacetexture
include $(CLEAR_VARS)

include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_C_INCLUDES +=                     \
    $(LOCAL_PATH)/include               \
    $(base-includes)                    \
    $(multimedia-base-includes)         \
    $(wayland-includes)                 \
    ${WINDOWSURFACE_INCLUDE}            \

ifeq ($(XMAKE_ENABLE_CNTR_HAL),true)
    LOCAL_C_INCLUDES += \
        $(multimedia-mediacodec-includes) \
        $(HYBRIS_EGLPLATFORMCOMMON)
else
    LOCAL_C_INCLUDES += \
            framework/libs/gui/surface \
            framework/libs/gui/surface/include \
            framework/libs/gui/surface/protocol \
            framework/libs/gui/surface/yunhal
endif

LOCAL_SRC_FILES += \
    src/media_surface_texture.cc \
    src/media_surface_texture_imp.cc \
    src/media_surface_utils.cc

LOCAL_LDLIBS:= -lstdc++
#LOCAL_CPPFLAGS += -std=c++11

LOCAL_SHARED_LIBRARIES += libmmbase

REQUIRE_WAYLAND = 1
REQUIRE_SURFACE = 1
REQUIRE_WPC = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

ifeq ($(USING_DRM_SURFACE),1)
    LOCAL_C_INCLUDES += $(MM_ROOT_PATH)/cow/src/platform/ivi/
    LOCAL_SHARED_LIBRARIES += libWlDrmSurface
endif

ifeq ($(USING_VPU_SURFACE),1)
    LOCAL_C_INCLUDES += $(MM_ROOT_PATH)/cow/src/platform/ivi/
    LOCAL_SHARED_LIBRARIES += libvpuSurface
endif

LOCAL_MODULE:= libmediasurfacetexture

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := mediasurfacetexture_multi_prebuilt
LOCAL_MODULE_TAGS := optional

MEDIASURFACETEXTURE_INCLUDE_HEADERS := \
    include/media_surface_texture.h:$(INST_INCLUDE_PATH)/media_surface_texture.h

LOCAL_SRC_FILES:= $(MEDIASURFACETEXTURE_INCLUDE_HEADERS)

include $(BUILD_MULTI_PREBUILT)

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

CURRENT_PATH:= $(call my-dir)
MM_ROOT_PATH:= $(CURRENT_PATH)/../../

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

## 0 for host codec, 1 for hybris codec
USE_MEDIA_COMPAT_LAYER := 0
## 0 for host WindowSurface, 1 for hybris stc
USE_HYBRIS_SURFACE := 0

## always link to libmedia_compat_layer.so even if USE_MEDIA_COMPAT_LAYER:0 & USE_HYBRIS_SURFACE:0
## since surface_texture_client_useBufferHandle() is used to pass GraphicsBuffer to OMX comp

## include $(graphics-includes) first to avoid search gl headers from hybris path
LOCAL_C_INCLUDES += \
        $(graphics-includes) \
        $(multimedia-base-includes) \
        $(libhybris-includes)       \
        $(base-includes)


LOCAL_CPPFLAGS += -DMP_HYBRIS_EGLPLATFORM="\"wayland\"" \
                  -fpermissive
LOCAL_SHARED_LIBRARIES += libmmbase
LOCAL_LDFLAGS += -lpthread -lstdc++

ifeq ($(USE_MEDIA_COMPAT_LAYER),1)
    LOCAL_CPPFLAGS += -D_USE_MEDIA_COMPAT_LAYER
else
    LOCAL_CPPFLAGS += -D__ENABLE_DEBUG__
endif

ifeq ($(USE_HYBRIS_SURFACE),1)
    LOCAL_CPPFLAGS += -D_USE_HYBRIS_SURFACE
else
    LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof
    LOCAL_C_INCLUDES += \
        $(HYBRIS_EGLPLATFORMCOMMON) \
        $(WINDOWSURFACE_INCLUDE)

    REQUIRE_WAYLAND = 1
    REQUIRE_SURFACE = 1
    REQUIRE_WPC = 1
endif
REQUIRE_LIBHYBRIS = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk


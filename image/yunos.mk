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

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_C_INCLUDES:= \
     $(LOCAL_PATH)/include       \
     $(MM_INCLUDE)               \
     $(MM_ROOT_PATH)/cow/include \
     $(base-includes)            \
     $(properties-includes)      \
     $(libjpeg-turbo-includes)   \
     $(multimedia-image-includes) \
     $(libpng-includes) \
     $(giflib-includes)           \
     $(libwebp-includes)          \
     $(libexif-includes)

ifeq ($(YUNOS_SYSCAP_RESMANAGER),true)
    LOCAL_C_INCLUDES += $(data-includes)
    LOCAL_CPPFLAGS += -DUSE_RESMANAGER
else
    LOCAL_C_INCLUDES += $(resourcelocator-includes)
    REQUIRE_RESOURCELOCATOR = 1
endif

LOCAL_SRC_FILES:= src/bmp.cc \
                  src/rgb.cc \
                  src/wbmp.cc \
                  src/regiondecoder.cc \
                  src/regiondecoder_jpg.cc \
                  src/image.cc \
                  src/jpeg_decoder.cc \
                  src/image_decoder.cc \
                  src/png_decoder.cc \
                  src/gif_decoder.cc \
                  src/webp_decoder.cc \
                  src/exif.cc \
                  src/exif_imp.cc

LOCAL_MODULE:= libimage

LOCAL_LDFLAGS += -lstdc++
LOCAL_SHARED_LIBRARIES += libmmbase

REQUIRE_LIBJPEG_TURBO := 1
REQUIRE_PNG := 1
REQUIRE_ZLIB :=1
REQUIRE_GIF := 1
REQUIRE_LIBWEBP := 1
REQUIRE_EXIF := 1

include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)

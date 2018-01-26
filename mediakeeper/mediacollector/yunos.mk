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
MM_ROOT_PATH:= $(LOCAL_PATH)/../../

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

ifeq ($(XMAKE_ENABLE_LOW_MEMORY),true)
LOCAL_CPPFLAGS += -DMEDIASCAN_SINGLE_THREAD
LOCAL_CPPFLAGS += -D_MEDIASCAN_PIC_MAX_SIZE=5000000
endif

LOCAL_C_INCLUDES:=            \
     $(LOCAL_PATH)/include    \
     $(LOCAL_PATH)/../include \
     $(MM_INCLUDE)            \
     $(base-includes)         \
     $(multimedia-image-includes) \
     $(libexif-includes)          \
     $(libjpeg-turbo-includes)    \
     $(libpng-includes)           \
     $(giflib-includes)           \
     $(libwebp-includes)          \
     $(libav-includes)

LOCAL_SRC_FILES:= src/mediacollector_imp.cc \
                  src/mediacollector_main.cc \
                  src/mediacollector.cc \
                  src/mediacollector_char_converter.cc

LOCAL_MODULE:= libmediacollector

LOCAL_SHARED_LIBRARIES += libmmbase libmediametar libimage
LOCAL_CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

LOCAL_LDFLAGS += -lstdc++ -lm
REQUIRE_EXIF := 1
REQUIRE_PNG := 1
REQUIRE_GIF := 1
REQUIRE_LIBJPEG_TURBO := 1
REQUIRE_LIBWEBP := 1
REQUIRE_ZLIB :=1
REQUIRE_LIBAV = 1
include  $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk
include $(BUILD_SHARED_LIBRARY)


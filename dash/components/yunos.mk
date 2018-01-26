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

include $(CLEAR_VARS)

COW_PLUGIN_PATH := /usr/lib/cow

#LIBDASH_C_INC := $(LOCAL_PATH)/../libdash/libdash/libdash/include/

#LIBDASH_FRAMEWORK_INC_PATH := $(LOCAL_PATH)/../libdash/libdash/qtsampleplayer/libdashframework
LIBDASH_FRAMEWORK_INC_PATH := $(libdash-includes)/../../qtsampleplayer/libdashframework/

DASHMANAGER_INC_PATH := $(LOCAL_PATH)/../manager/

LIBDASH_FRAMEWORK_INC :=                                         \
     $(LIBDASH_FRAMEWORK_INC_PATH)/Adaptation/                   \
     $(LIBDASH_FRAMEWORK_INC_PATH)/Buffer/                       \
     $(LIBDASH_FRAMEWORK_INC_PATH)/helpers/                      \
     $(LIBDASH_FRAMEWORK_INC_PATH)/Input/                        \
     $(LIBDASH_FRAMEWORK_INC_PATH)/MPD/                          \
     $(LIBDASH_FRAMEWORK_INC_PATH)/Portable/                     \

LOCAL_C_INCLUDES := $(DASHMANAGER_INC_PATH)                                           \
                    $(LIBDASH_FRAMEWORK_INC)                                          \
                    $(libdash-includes)                                               \
                    $(base-includes)                                                  \
                    $(libav-includes)                                                 \
                    $(multimedia-base-includes)                                       \
                    $(YUNOS_ROOT)/framework/nativeservice/multimediad/cow/include     \
                    $(YUNOS_ROOT)/framework/nativeservice/multimediad/cow/src         \
                    $(base-includes)                                                  \
                    $(multimedia-mediarecorder-includes)

LOCAL_MODULE_PATH = $(COW_PLUGIN_PATH)

LOCAL_SRC_FILES = seg_extractor.cc

LOCAL_MODULE := libSegExtractor

LOCAL_SHARED_LIBRARIES += libdash_stream libdashframework libmmbase libcowbase libcow-avhelper
LOCAL_SHARED_LIBRARIES += libavformat libavcodec libavutil
#REQUIRE_LIBAV = 1
#include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_LDFLAGS += -lstdc++ -lpthread -std=c++11
#LOCAL_CPPFLAGS += -std=c++11

LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof
LOCAL_CPPFLAGS += -D_COW_SOURCE_VERSION=\"0.5.3:20170322\"

include $(BUILD_SHARED_LIBRARY)

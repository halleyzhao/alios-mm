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
MM_ROOT_PATH:= $(LOCAL_PATH)/../../../

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_SRC_FILES:= \
    src/PCMPlayer.cc \
    src/PCMPlayerIMP.cc

LOCAL_MODULE:= libpcmplayer

LOCAL_C_INCLUDES:=               \
     $(LOCAL_PATH)/include       \
     $(LOCAL_PATH)/../../include \
     $(MM_INCLUDE)               \
     $(corefoundation-includes)  \
     $(base-includes)

ifeq ($(ENABLE_DEFAULT_AUDIO_CONNECTION),1)
LOCAL_C_INCLUDES += ../../../ext/mm_amhelper/include
LOCAL_SHARED_LIBRARIES += libmmamhelper
endif

LOCAL_LDFLAGS += -lstdc++ -lpthread
ifeq ($(USING_AUDIO_CRAS), 1)
LOCAL_C_INCLUDES += $(audioserver-includes)
LOCAL_CPPFLAGS += -D__AUDIO_CRAS__
REQUIRE_AUDIOSERVER = 1
endif

ifeq ($(USING_AUDIO_PULSE), 1)
LOCAL_CPPFLAGS += -D__AUDIO_PULSE__
LOCAL_C_INCLUDES += $(pulseaudio-includes)
REQUIRE_PULSEAUDIO = 1
endif


LOCAL_SHARED_LIBRARIES += libmmbase

include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)



include $(CLEAR_VARS)
LOCAL_MODULE := mmpcmp_multi_prebuilt
LOCAL_MODULE_TAGS := optional
MMPCMP_INCLUDE_PATH := $(LOCAL_PATH)/../../include/multimedia
MMPCMP_INCLUDE_HEADERS := \
    $(MMPCMP_INCLUDE_PATH)/PCMPlayer.h:$(INST_INCLUDE_PATH)/PCMPlayer.h

LOCAL_SRC_FILES:= $(MMPCMP_INCLUDE_HEADERS)

include $(BUILD_MULTI_PREBUILT)


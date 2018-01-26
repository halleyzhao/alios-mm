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

LOCAL_SRC_FILES:= \
    src/channel.cc \
    src/loader.cc \
    src/instantaudio.cc \
    src/instantaudioimp.cc \
    src/sample.cc

LOCAL_MODULE:= libinstantaudio

LOCAL_C_INCLUDES:=         \
     $(LOCAL_PATH)/include \
     $(LOCAL_PATH)/src     \
     $(MM_INCLUDE)         \
     $(MM_ROOT_PATH)/audio/include  \
     $(base-includes)               \
     $(pulseaudio-includes)         \
     $(audioserver-includes)        \
     $(corefoundation-includes)

ifeq ($(ENABLE_DEFAULT_AUDIO_CONNECTION),1)
LOCAL_C_INCLUDES += ../ext/mm_amhelper/include
LOCAL_SHARED_LIBRARIES += libmmamhelper
endif

LOCAL_LDFLAGS += -lpthread -lstdc++
ifneq ($(filter $(XMAKE_PLATFORM), phone tablet tv lite), )
LOCAL_CPPFLAGS += -D__AUDIO_CRAS__
REQUIRE_AUDIOSERVER = 1
else
  ifeq ($(USING_AUDIO_CRAS),1)
    LOCAL_CPPFLAGS += -D__AUDIO_CRAS__
    REQUIRE_AUDIOSERVER = 1
  else
    LOCAL_CPPFLAGS += -D__AUDIO_PULSE__
    REQUIRE_PULSEAUDIO = 1
  endif
endif
LOCAL_SHARED_LIBRARIES += libmmbase libaudio_decode

include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)



include $(CLEAR_VARS)

LOCAL_MODULE := mminstantaudio_headers_prebuilt
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := usr/include/multimedia

MMSP_INCLUDE_HEADERS := \
    $(LOCAL_PATH)/include/multimedia/instantaudio.h

LOCAL_SRC_FILES:= $(MMSP_INCLUDE_HEADERS)

include $(BUILD_PREBUILT)


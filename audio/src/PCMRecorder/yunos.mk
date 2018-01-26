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
    src/PCMRecorder.cc \
    src/PCMRecorderIMP.cc

LOCAL_MODULE:= libpcmrecorder

LOCAL_C_INCLUDES:=               \
     $(LOCAL_PATH)/include       \
     $(LOCAL_PATH)/../../include \
     $(MM_INCLUDE)               \
     $(base-includes)            \
     $(audioserver-includes)     \
     $(corefoundation-includes)  \
     $(pulseaudio-includes)

ifeq ($(ENABLE_DEFAULT_AUDIO_CONNECTION),1)
LOCAL_C_INCLUDES += ../../../ext/mm_amhelper/include
LOCAL_SHARED_LIBRARIES += libmmamhelper
endif

LOCAL_LDFLAGS += -lstdc++
ifneq ($(filter $(XMAKE_PLATFORM), phone tablet tv ivi lite), )
ifeq ($(USING_AUDIO_CRAS),1)
REQUIRE_AUDIOSERVER = 1
else
REQUIRE_PULSEAUDIO=1
endif
endif

LOCAL_SHARED_LIBRARIES += libmmbase

include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := mmpcmr_multi_prebuilt
LOCAL_MODULE_TAGS := optional
MMPCMR_INCLUDE_PATH := $(LOCAL_PATH)/../../include/multimedia
MMPCMR_INCLUDE_HEADERS := $(MMPCMR_INCLUDE_PATH)/PCMRecorder.h:$(INST_INCLUDE_PATH)/PCMRecorder.h

LOCAL_SRC_FILES:= $(MMPCMR_INCLUDE_HEADERS)

include $(BUILD_MULTI_PREBUILT)

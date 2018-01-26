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

LOCAL_SRC_FILES:=      \
    src/SysSound.cc    \
    src/SysSoundImp.cc \
    src/SysSoundProxy.cc

LOCAL_MODULE:= libsyssound

LOCAL_C_INCLUDES:=         \
     $(LOCAL_PATH)/include \
     $(LOCAL_PATH)/src     \
     $(MM_INCLUDE)         \
     $(base-includes)      \
     $(dbus-includes)      \
     $(libuv-includes)


LOCAL_LDFLAGS += -lstdc++  -pthread
LOCAL_SHARED_LIBRARIES += libmmbase

include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)

#################################
# libsyssoundservice

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_SRC_FILES:=          \
    src/SysSoundService.cc \
    src/SysSoundServiceAdaptor.cc

LOCAL_MODULE:= libsyssoundservice

LOCAL_C_INCLUDES:=                    \
     $(LOCAL_PATH)/include            \
     $(LOCAL_PATH)/src                \
     $(MM_INCLUDE)                    \
     $(base-includes)                 \
     $(dbus-includes)                 \
     $(libuv-includes)                \
     $(multimedia-instantaudio-includes) \
     $(audioserver-includes)          \
     $(alsa_lib-includes)

LOCAL_LDFLAGS += -lstdc++  -pthread
LOCAL_SHARED_LIBRARIES += libmmbase libinstantaudio

REQUIRE_EXPAT := 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)


#############################
# config
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
LOCAL_MODULE := mm_soundservice_multi_prebuilt

LOCAL_MODULE_PATH:= etc
ifeq ($(XMAKE_ENABLE_LOW_MEMORY),true)
  LOCAL_SRC_FILES:= src/syssound_small_mem.xml:syssound.xml
else
  LOCAL_SRC_FILES:= src/syssound.xml:syssound.xml
endif
include $(BUILD_MULTI_PREBUILT)


#############################
# resource
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
LOCAL_MODULE := mm_soundservice_multi_prebuilt1

ifeq ($(XMAKE_ENABLE_LOW_MEMORY),true)
RESOURCE_PATH += resource/small_mem
else
RESOURCE_PATH := resource
endif

LOCAL_MODULE_PATH:= usr/data/media/audio/ui
LOCAL_SRC_FILES:= $(RESOURCE_PATH)/Effect_Tick.ogg:Effect_Tick.ogg \
        $(RESOURCE_PATH)/KeypressInvalid.ogg:KeypressInvalid.ogg \
        $(RESOURCE_PATH)/KeypressDelete.ogg:KeypressDelete.ogg \
        $(RESOURCE_PATH)/KeypressReturn.ogg:KeypressReturn.ogg \
        $(RESOURCE_PATH)/KeypressSpacebar.ogg:KeypressSpacebar.ogg \
        $(RESOURCE_PATH)/KeypressStandard.ogg:KeypressStandard.ogg \
        $(RESOURCE_PATH)/Lock.ogg:Lock.ogg \
        $(RESOURCE_PATH)/Unlock.ogg:Unlock.ogg
include $(BUILD_MULTI_PREBUILT)


ifeq ($(XMAKE_PRODUCT),$(filter $(XMAKE_PRODUCT),yunhal yunbot))

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_SRC_FILES:= src/main.cc

LOCAL_C_INCLUDES += \
    $(MM_INCLUDE) \
    $(multimedia-syssound-includes)

LOCAL_LDFLAGS += -lpthread -lstdc++

LOCAL_SHARED_LIBRARIES += libmmbase libsyssoundservice

LOCAL_MODULE:= media_service

include $(BUILD_EXECUTABLE)

endif


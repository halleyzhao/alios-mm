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

ifeq ($(USING_PLAYBACK_CHANNEL),1)
LOCAL_PATH:= $(call my-dir)
MC_ROOT_PATH:= framework/nativeservice/multimediad/base

include $(CLEAR_VARS)
include $(MC_ROOT_PATH)/build/build.mk

LOCAL_SRC_FILES:=      \
    src/PlaybackEvent.cc    \
    src/PlaybackState.cc \
    src/PlaybackChannelManager.cc \
    src/PlaybackChannelManagerImp.cc \
    src/PlaybackRemote.cc \
    src/PlaybackRemoteImp.cc \
    src/PlaybackChannel.cc \
    src/PlaybackChannelImp.cc


LOCAL_MODULE:= libpbchannel

LOCAL_C_INCLUDES +=         \
     $(LOCAL_PATH)/include \
     $(LOCAL_PATH)/src     \
     $(multimedia-base-includes)         \
     $(base-includes)      \
     $(dbus-includes)      \
     $(libuv-includes)     \
     $(dynamicpagemanager-includes) \
     $(page-includes)


ifeq ($(USING_PLAYBACK_CHANNEL_FUSION),1)
LOCAL_SHARED_LIBRARIES += libcontainermanagerwrapper
endif

LOCAL_LDFLAGS += -lstdc++  -pthread
LOCAL_SHARED_LIBRARIES += libmmbase libdpms-proxy

REQUIRE_PERMISSION = 1
include $(MC_ROOT_PATH)/build/xmake_req_libs.mk

include $(BUILD_SHARED_LIBRARY)

#################################
# PlaybackChannelService

include $(CLEAR_VARS)
include $(MC_ROOT_PATH)/build/build.mk

LOCAL_SRC_FILES:=          \
    src/PlaybackChannelService.cc \
    src/PlaybackChannelServiceImp.cc \
    src/PlaybackRecordList.cc \
    src/PlaybackRecord.cc \
    src/PlaybackState.cc \
    src/PlaybackEvent.cc \


LOCAL_MODULE:= libpbchannelservice

LOCAL_C_INCLUDES:=                    \
     $(LOCAL_PATH)/include            \
     $(LOCAL_PATH)/src                \
     $(multimedia-base-includes)                    \
     $(base-includes)                 \
     $(dbus-includes)                 \
     $(libuv-includes)                \
     $(permission-includes)           \
     $(dynamicpagemanager-includes)   \
     $(page-includes)                 \


LOCAL_LDFLAGS += -lstdc++  -pthread
LOCAL_SHARED_LIBRARIES += libmmbase \
                          libdpms-proxy \
                          libpage

REQUIRE_EXPAT := 1
REQUIRE_PERMISSION = 1
include $(MC_ROOT_PATH)/build/xmake_req_libs.mk
include $(BUILD_SHARED_LIBRARY)

#################################
# PlaybackChannelservice dumpinfo

include $(CLEAR_VARS)
include $(MC_ROOT_PATH)/build/build.mk

LOCAL_SRC_FILES:=          \
    src/dumpsys.cc

LOCAL_MODULE:= dumpsys_pbchannel

LOCAL_C_INCLUDES:=                    \
     $(LOCAL_PATH)/include            \
     $(LOCAL_PATH)/src                \
     $(multimedia-base-includes)                    \
     $(base-includes)                 \



LOCAL_LDFLAGS += -lstdc++  -pthread
LOCAL_SHARED_LIBRARIES += libmmbase libpbchannel

include $(BUILD_EXECUTABLE)
endif

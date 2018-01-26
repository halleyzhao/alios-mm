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

## FIXME, use better filter
ifeq ($(USING_PLAYBACK_CHANNEL),1)
LOCAL_PATH:=$(call my-dir)
MM_ROOT_PATH:= $(LOCAL_PATH)/../../

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SRC_FILES:= PlaybackChannelManagerTest.cc PlaybackChannelTestHelper.cc

LOCAL_C_INCLUDES += \
    $(MM_INCLUDE) \
    $(audioserver-includes) \
    $(multimedia-pbchannel-includes)



LOCAL_LDFLAGS += -lpthread -lstdc++

LOCAL_SHARED_LIBRARIES += libmmbase libpbchannel

LOCAL_MODULE:= pbchannelmanager-test

include $(BUILD_EXECUTABLE)


###############################################
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SRC_FILES:= PlaybackChannelTest.cc \
				  PlaybackChannelTestHelper.cc

LOCAL_C_INCLUDES += \
    $(MM_INCLUDE) \
    $(audioserver-includes) \
    $(multimedia-pbchannel-includes)



LOCAL_LDFLAGS += -lpthread -lstdc++

LOCAL_SHARED_LIBRARIES += libmmbase libpbchannel

LOCAL_MODULE:= pbchannel-test

include $(BUILD_EXECUTABLE)



###############################################
##pbremoteipc-test
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SRC_FILES:= PlaybackRemoteIpcTest.cc  PlaybackChannelTestHelper.cc

LOCAL_C_INCLUDES += \
    $(MM_INCLUDE) \
    $(audioserver-includes) \
    $(multimedia-pbchannel-includes)



LOCAL_LDFLAGS += -lpthread -lstdc++

LOCAL_SHARED_LIBRARIES += libmmbase libpbchannel

LOCAL_MODULE:= pbremoteipc-test

include $(BUILD_EXECUTABLE)


############################################
##pbremote-test
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SRC_FILES:= PlaybackRemoteTest.cc  PlaybackChannelTestHelper.cc

LOCAL_C_INCLUDES += \
    $(MM_INCLUDE) \
    $(audioserver-includes) \
    $(multimedia-pbchannel-includes)

LOCAL_LDFLAGS += -lpthread -lstdc++

LOCAL_SHARED_LIBRARIES += libmmbase libpbchannel

LOCAL_MODULE:= pbremote-test

include $(BUILD_EXECUTABLE)


endif

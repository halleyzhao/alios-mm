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

ifeq ($(XMAKE_ENABLE_CNTR_HAL),true)

LOCAL_PATH:=$(call my-dir)
MM_ROOT_PATH:= $(LOCAL_PATH)/../../

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/cow/build/cow_common.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SRC_FILES := wifidisplay_source_test.cc
LOCAL_C_INCLUDES += $(MM_ROOT_PATH)/cow/build \
                    $(WINDOWSURFACE_INCLUDE) \
                    $(cameraserver-includes)   \
                    $(corefoundation-includes) \
                    $(MM_ROOT_PATH)/wifidisplay/include \
                    $(MM_ROOT_PATH)/cow/include \
                    $(MM_ROOT_PATH)/mediaplayer/include/      \
                    $(MM_ROOT_PATH)/mediarecorder/include/


LOCAL_LDFLAGS += -lpthread
LOCAL_SHARED_LIBRARIES += libwifi_display
REQUIRE_CAMERASVR = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk

LOCAL_MODULE := wfdsource-test
LOCAL_LDFLAGS += -lstdc++
include $(BUILD_EXECUTABLE)

##############################################################

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/cow/build/cow_common.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk

LOCAL_SRC_FILES := wifidisplay_sink_test.cc
LOCAL_C_INCLUDES += $(MM_ROOT_PATH)/cow/build \
                    $(MM_ROOT_PATH)/wifidisplay/include \
                    $(MM_ROOT_PATH)/cow/include \
                    $(MM_ROOT_PATH)/mediaplayer/include/      \
                    $(MM_ROOT_PATH)/mediarecorder/include/

LOCAL_C_INCLUDES+= \
    $(audioserver-includes)

LOCAL_LDFLAGS += -lpthread
LOCAL_SHARED_LIBRARIES += libwifi_display

LOCAL_MODULE := wfdsink-test
LOCAL_LDFLAGS += -lstdc++
include $(BUILD_EXECUTABLE)

endif

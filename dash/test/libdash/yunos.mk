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


#LIBDASH_C_INC := $(LOCAL_PATH)/../../libdash/libdash/libdash/include/

#LIBDASH_FRAMEWORK_INC_PATH := $(LOCAL_PATH)/../../libdash/libdash/qtsampleplayer/libdashframework
LIBDASH_FRAMEWORK_INC_PATH := $(libdash-includes)/../../qtsampleplayer/libdashframework/

LIBDASH_FRAMEWORK_INC :=                                         \
     $(LIBDASH_FRAMEWORK_INC_PATH)/Adaptation/                   \
     $(LIBDASH_FRAMEWORK_INC_PATH)/Buffer/                       \
     $(LIBDASH_FRAMEWORK_INC_PATH)/helpers/                      \
     $(LIBDASH_FRAMEWORK_INC_PATH)/Input/                        \
     $(LIBDASH_FRAMEWORK_INC_PATH)/MPD/                          \
     $(LIBDASH_FRAMEWORK_INC_PATH)/Portable/                     \


LOCAL_C_INCLUDES := $(libdash-includes)
LOCAL_C_INCLUDES += $(LIBDASH_FRAMEWORK_INC)

LOCAL_SRC_FILES = $(wildcard $(LOCAL_PATH)/*.cpp)

LOCAL_MODULE := libdash-test

LOCAL_SHARED_LIBRARIES += libdash libdashframework

LOCAL_LDFLAGS += -lstdc++ -lpthread -std=c++11

LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof

include $(BUILD_EXECUTABLE)

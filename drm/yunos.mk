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

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                               \
        src/media_keys.cc                       \
        src/media_key_session.cc                \
        src/media_key_utils.cc                  \
        src/media_key_event.cc                  \
        src/media_decrypt.cc                    \
        src/adaptor/media_key_factory.cc

LOCAL_C_INCLUDES :=               \
        $(LOCAL_PATH)/src         \
        $(LOCAL_PATH)/src/adaptor \
        $(base-includes)

##LOCAL_SHARED_LIBRARIES := \
        liblog libdl libpthread

LOCAL_SHARED_LIBRARIES := liblog

LOCAL_MODULE := libmediadrm_yunos

LOCAL_MODULE_TAGS := optional

LOCAL_LDFLAGS += -lpthread -ldl -lstdc++

include $(BUILD_SHARED_LIBRARY)

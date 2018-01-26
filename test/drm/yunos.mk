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
LOCAL_PATH:= $(call my-dir)
MM_ROOT_PATH:= $(LOCAL_PATH)/../../

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=  wasabi_engine_simple_test.cc


LOCAL_C_INCLUDES := $(MM_ROOT_PATH)/drm/src   \
                    $(base-includes)

LOCAL_SHARED_LIBRARIES += libmediadrm_yunos

LOCAL_MODULE := marlin-simple-test

LOCAL_MODULE_TAGS := optional

LOCAL_LDFLAGS += -lstdc++ -lmediadrm_yunos

include $(BUILD_EXECUTABLE)

#########################widevine simple test
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= widevine_simple_test.cc


LOCAL_C_INCLUDES := $(MM_ROOT_PATH)/drm/src \
                    $(base-includes)

LOCAL_SHARED_LIBRARIES += libmediadrm_yunos

LOCAL_MODULE := widevine-simple-test

LOCAL_MODULE_TAGS := optional

LOCAL_LDFLAGS += -lstdc++

include $(BUILD_EXECUTABLE)

endif

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
MM_ROOT_PATH:= $(LOCAL_PATH)/../../

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk

LOCAL_C_INCLUDES:=            \
     $(LOCAL_PATH)/include    \
     $(LOCAL_PATH)/../include \
     $(MM_INCLUDE)            \
     $(MM_ROOT_PATH)/cow/include \
     $(base-includes) \
     $(multimedia-image-includes) \
     $(libav-includes)

SRC_PATH := src

LOCAL_SRC_FILES:= $(SRC_PATH)/mediametaretriever.cc \
                  $(SRC_PATH)/mediametaretriever_imp.cc

LOCAL_MODULE:= libmediametar

LOCAL_SHARED_LIBRARIES += libmmbase libcowbase libimage
REQUIRE_LIBAV = 1
include  $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk
LOCAL_LDFLAGS += -lstdc++
include $(BUILD_SHARED_LIBRARY)


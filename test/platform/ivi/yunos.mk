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


LOCAL_PATH:=$(call my-dir)
MM_ROOT_PATH:= $(LOCAL_PATH)/../../../

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
include $(MM_ROOT_PATH)/test/gtest_common.mk


LOCAL_C_INCLUDES+=  ${TI-LIBDCE-INCLUDES}   \
                    $(ti-ipc-includes)      \
                    $(libdrm-includes)      \
                    $(wayland-includes)     \
                    $(MM_ROOT_PATH)/cow/src/platform/ivi/

LOCAL_SHARED_LIBRARIES += libmmbase libVpeFilter
LOCAL_LDFLAGS += -lstdc++ -lpthread

LOCAL_SRC_FILES:= vpe_filter_test.cc

LOCAL_C_INCLUDES += \
    $(MM_INCLUDE) \
    $(base-includes)

LOCAL_SHARED_LIBRARIES += libmmbase

LOCAL_LDFLAGS += -lstdc++ -lpthread
REQUIRE_LIBDCE = 1
REQUIRE_LIBDRM = 1
REQUIRE_LIBMMRPC = 1
include $(MM_ROOT_PATH)/base/build/xmake_req_libs.mk
ifeq ($(USING_DRM_SURFACE),1)
LOCAL_MODULE:= vpe-test
endif

include $(BUILD_EXECUTABLE)

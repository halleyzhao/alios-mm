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

BASE_BUILD_DIR:=$(MULTIMEDIA_BASE)/base/build
include $(BASE_BUILD_DIR)/reset_args

LOCAL_DEFINES += -D_KEEP_AVCC_DATA_FORMAT
LOCAL_INCLUDES:= ./ $(MM_INCLUDE) $(MM_MEDIACODEC_INCLUDE)
LOCAL_INCLUDES += $(MULTIMEDIA_BASE)/mcoverv4l2/src
LOCAL_INCLUDES += $(MM_HELP_WINDOWSURFACE_INCLUDE)
LOCAL_SHARED_LIBRARIES := pthread gtest gtest_main mmbase mediacodec_v4l2 dl  stdc++

LOCAL_CPPFLAGS := -g -D__ENABLE_X11__ `pkg-config --cflags libavformat libavcodec libavutil`
LOCAL_LDFLAGS:= `pkg-config --libs libavformat libavcodec libavutil`
LOCAL_CPPFLAGS += $(GLIB_G_OPTION_CFLAGS)
LOCAL_LDFLAGS += $(GLIB_G_OPTION_LDFLAGS)

LOCAL_MODULE := mediacodec-test

LOCAL_SRC_FILES := mediacodecPlay.cc            \
                   mediacodectest.cc

MODULE_TYPE := eng
include $(BASE_BUILD_DIR)/build_exec


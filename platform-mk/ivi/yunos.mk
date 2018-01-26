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


# include $(call all-subdir-makefiles)
MY_IVI_MM_ROOT_DIR:=$(call my-dir)/../..

## base
include ${MY_IVI_MM_ROOT_DIR}/base/yunos.mk
include ${MY_IVI_MM_ROOT_DIR}/cow/yunos.mk

## module 1
include ${MY_IVI_MM_ROOT_DIR}/ext/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/mediasurfacetexture/yunos.mk
include ${MY_IVI_MM_ROOT_DIR}/mediaplayer/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/mediarecorder/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/wifidisplay/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/mediaserver/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/audio/yunos.mk

## module 2
# include ${MY_IVI_MM_ROOT_DIR}/pbchannel/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/mediaprovider/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/syssound/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/image/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/webrtc/yunos.mk

## misc
# include ${MY_IVI_MM_ROOT_DIR}/drm/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/mcv4l2/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/v4l2deviceservice/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/transcoding/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/vrvideoview/yunos.mk

## mdk
# include ${MY_IVI_MM_ROOT_DIR}/examples/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/examples/comps/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/mdk/yunos.mk

## test
include ${MY_IVI_MM_ROOT_DIR}/test/cow/simpleplayer/yunos.mk
include ${MY_IVI_MM_ROOT_DIR}/test/cow/recorder/yunos.mk

# include ${MY_IVI_MM_ROOT_DIR}/test/yunos.mk
# include ${MY_IVI_MM_ROOT_DIR}/test/webrtc/y.mk



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

###install ut test resources

####test shell
include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
LOCAL_MODULE := mm_test_multi_prebuilt
LOCAL_MODULE_TAGS := optional

MM_TEST_SH_INSTALL := \
    test_cowplayer_full.sh:$(INST_TEST_SH_PATH)/test_cowplayer_full.sh \
    test_mmbase.sh:$(INST_TEST_SH_PATH)/test_mmbase.sh                 \
    test_mmaudio.sh:$(INST_TEST_SH_PATH)/test_mmaudio.sh               \
    test_instantaudio.sh:$(INST_TEST_SH_PATH)/test_instantaudio.sh           \
    test_cow.sh:$(INST_TEST_SH_PATH)/test_cow.sh                       \
    test_mediacodec.sh:$(INST_TEST_SH_PATH)/test_mediacodec.sh         \
    test_mediaplayer.sh:$(INST_TEST_SH_PATH)/test_mediaplayer.sh       \
    test_mm_full.sh:$(INST_TEST_SH_PATH)/test_mm_full.sh               \
    use_ffmpeg_plugins.xml:$(INST_TEST_SH_PATH)/use_ffmpeg_plugins.xml \
    test_pbchannel.sh:$(INST_TEST_SH_PATH)/test_pbchannel.sh     \
    test_dash.sh:$(INST_TEST_SH_PATH)/test_dash.sh


LOCAL_SRC_FILES:= $(MM_TEST_SH_INSTALL)

include $(BUILD_MULTI_PREBUILT)


####audio and video

include $(CLEAR_VARS)
include $(MM_ROOT_PATH)/base/build/build.mk
LOCAL_MODULE := mm_test_res_multi_prebuilt
LOCAL_MODULE_TAGS := optional

AUDIO_RES_SRC_PATH := audio
VIDEO_RES_SRC_PATH := video
PCM_RES_SRC_PATH := $(AUDIO_RES_SRC_PATH)/audiocompat/pcms
PCM_RES_DST_PATH := $(INST_TEST_RES_PATH)/audio/audiocompat/pcms

IMG_RES_SRC_PATH := img
IMG_RES_DST_PATH := $(INST_TEST_RES_PATH)/img

MM_TEST_RES_INSTALL := \
    $(AUDIO_RES_SRC_PATH)/yjm.mp3:$(INST_TEST_RES_PATH)/audio/yjm.mp3 \
    $(AUDIO_RES_SRC_PATH)/ad.mp3:$(INST_TEST_RES_PATH)/audio/ad.mp3 \
    $(AUDIO_RES_SRC_PATH)/sp.mp3:$(INST_TEST_RES_PATH)/audio/sp.mp3 \
    $(AUDIO_RES_SRC_PATH)/sp2.mp3:$(INST_TEST_RES_PATH)/audio/sp2.mp3 \
    $(AUDIO_RES_SRC_PATH)/tick.ogg:$(INST_TEST_RES_PATH)/audio/tick.ogg \
    $(AUDIO_RES_SRC_PATH)/dts.mp4:$(INST_TEST_RES_PATH)/audio/dts.mp4 \
    $(AUDIO_RES_SRC_PATH)/aac.aac:$(INST_TEST_RES_PATH)/audio/aac.aac \
    $(PCM_RES_SRC_PATH)/__[11025_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[11025_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[32000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[32000_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[8000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[8000_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[12000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[12000_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[44100_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[44100_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[88200_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[88200_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[16000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[16000_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[48000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[48000_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[96000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[96000_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[22050_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[22050_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[6000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[6000_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[24000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[24000_S16LE_2].pcm \
    $(PCM_RES_SRC_PATH)/__[64000_S16LE_2].pcm:$(PCM_RES_DST_PATH)/__[64000_S16LE_2].pcm \
    $(VIDEO_RES_SRC_PATH)/test.mp4:$(INST_TEST_RES_PATH)/video/test.mp4   \
    $(VIDEO_RES_SRC_PATH)/test.webm:$(INST_TEST_RES_PATH)/video/test.webm \
    $(VIDEO_RES_SRC_PATH)/h264.mp4:$(INST_TEST_RES_PATH)/video/h264.mp4 \
    $(VIDEO_RES_SRC_PATH)/trailer_short.mp4:$(INST_TEST_RES_PATH)/video/trailer_short.mp4 \
    $(VIDEO_RES_SRC_PATH)/short.mp4:$(INST_TEST_RES_PATH)/video/short.mp4 \
    $(VIDEO_RES_SRC_PATH)/yuv420_320x240.yuv:$(INST_TEST_RES_PATH)/video/yuv420_320x240.yuv \
    $(VIDEO_RES_SRC_PATH)/test03.srt:$(INST_TEST_RES_PATH)/video/test03.srt \
    $(VIDEO_RES_SRC_PATH)/test04.txt:$(INST_TEST_RES_PATH)/video/test04.txt \
    $(VIDEO_RES_SRC_PATH)/test05.smi:$(INST_TEST_RES_PATH)/video/test05.smi \
    $(VIDEO_RES_SRC_PATH)/test06.ssa:$(INST_TEST_RES_PATH)/video/test06.ssa \
    $(VIDEO_RES_SRC_PATH)/test07.ass:$(INST_TEST_RES_PATH)/video/test07.ass \
    $(IMG_RES_SRC_PATH)/jpg.jpg:$(IMG_RES_DST_PATH)/jpg.jpg \
    $(IMG_RES_SRC_PATH)/png.png:$(IMG_RES_DST_PATH)/png.png \
    $(IMG_RES_SRC_PATH)/bmp.bmp:$(IMG_RES_DST_PATH)/bmp.bmp \
    $(IMG_RES_SRC_PATH)/wbmp.wbmp:$(IMG_RES_DST_PATH)/wbmp.wbmp \
    $(IMG_RES_SRC_PATH)/gif.gif:$(IMG_RES_DST_PATH)/gif.gif \
    $(IMG_RES_SRC_PATH)/webp.webp:$(IMG_RES_DST_PATH)/webp.webp \

LOCAL_SRC_FILES:= $(MM_TEST_RES_INSTALL)

include $(BUILD_MULTI_PREBUILT)



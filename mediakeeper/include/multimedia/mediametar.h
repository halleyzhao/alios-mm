/**
 * Copyright (C) 2017 Alibaba Group Holding Limited. All Rights Reserved.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __mediametar_H
#define __mediametar_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	char *artist;		/* A pointer to Artist */
	char *album;		/* A pointer to Album name. */
	char *genre;		/* A pointer to Genre */
	char *author;		/* A pointer to Author name */
	char *copyright;		/* A pointer to Copyright info */
	char *year;			/* A pointer to release year */
	char *description;		/* A pointer to description */
	int duration;		/* Duration of the track */
	int audio_bitrate;	/* Audio bitrate */
	int sample_rate;
	int num_channel;
	int audio_codec;
	int rating;
} common_meta_tt;

typedef struct {
	int video_fps;
	int video_br;
	int video_h;
	int video_w;
	char *track;
} video_meta_tt;

typedef struct {
	int track;
} audio_meta_tt;

typedef struct {
	common_meta_tt commonmeta;
	audio_meta_tt audiometa;
} comp_audio_meta_tt;

typedef struct {
	common_meta_tt commonmeta;
	video_meta_tt videometa;
} comp_video_meta_tt;

int extractAudioFileMetadata(char * file, comp_audio_meta_tt *audio_meta);
int extractVideoFileMetadata(char * file, comp_video_meta_tt *video_meta);
void releaseMediaMetaRetriever();

#ifdef __cplusplus
};
#endif

#endif // __mediametar_H



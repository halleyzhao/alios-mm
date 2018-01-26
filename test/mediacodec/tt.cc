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

#include <stdio.h>
#include <media_codec_hbs.h>
#include <compat/media/surface_texture_client_compat.h>

int main()
{
    SurfaceTextureClientHybris  stc= NULL;
    YunOSMediaCodec::MediaCodec* decoder = NULL;

    printf("it does nothing, but to verify mediacodec package are correctly installed");
    stc = surface_texture_client_create();
    decoder = YunOSMediaCodec::MediaCodec::CreateByType("video/x-264",false);
    decoder->release();
    surface_texture_client_destroy(stc);

    printf("test done\n");
}

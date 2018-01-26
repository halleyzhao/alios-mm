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

#ifndef __utils_H
#define __utils_H

#define ALPHA_DEF_VAL 255

#define RGB565_2_32_S(_rgb, _565) do {\
    (_rgb)->blue = ((*_565) & 0xf8) | (((*_565) >> 3) & 0x7);\
    (_rgb)->green = (((*_565) & 0x7) << 5) | \
                        ((*(_565+1) >> 3) & 0x1c) |\
                        ((*(_565+1) >> 5) & 0x3);\
    (_rgb)->red = (((*(_565+1)) << 3) & 0xf8) |\
                    ((*(_565+1)) & 0x7);\
    (_rgb)->alpha = ALPHA_DEF_VAL;\
}while(0)


#define RGB565_2_32(_rgb, _565) do {\
    (*(_rgb)++) = ((*_565) & 0xf8) | (((*_565) >> 3) & 0x7);\
    (*(_rgb)++) = (((*_565) & 0x7) << 5) | \
                        ((*(_565+1) >> 3) & 0x1c) |\
                        ((*(_565+1) >> 5) & 0x3);\
    (*(_rgb)++) = (((*(_565+1)) << 3) & 0xf8) |\
                    ((*(_565+1)) & 0x7);\
    (*(_rgb)++) = ALPHA_DEF_VAL;\
    _565 += 2;\
}while(0)


#define RGB_2_565(_r, _g, _b, _565) do {\
    *(_565++) = ((_b) & 0xF8) | (((_g) >> 5) & 0x7);\
    *(_565++) = (((_g) << 3) & 0xE0) | (((_r) >> 3) & 0x1f);\
}while(0)

#endif

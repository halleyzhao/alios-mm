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


#include <gif_lib.h>
#include "gif_decoder.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(GifDecoder)

MMBufferSP GifDecoder::decode(/* [in] */const MMBufferSP & srcBuf,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    MMLOGE("MM_ERROR_UNSUPPORTED\n");
    return MMBufferSP((MMBuffer*)NULL);
}

MMBufferSP GifDecoder::decode(/* [in] */const uint8_t * srcBuf,
                    /*[in]*/size_t srcBufSize,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    MMLOGE("MM_ERROR_UNSUPPORTED\n");
    return MMBufferSP((MMBuffer*)NULL);
}



#define RELEASE_SCREEN_BUF(_sb, _count) do {\
    if (!_sb) {\
        break;\
    }\
    for (int i = 0; i < _count; ++i) {\
        GifRowType p = _sb[i];\
        if (p) {\
/*            MMLOGV("releasing(%d/%d): %p\n", i, _count, p);*/\
            free(p);\
            _sb[i] = NULL;\
/*            MMLOGV("releasing(%d/%d): %p over\n", i, _count, p);*/\
        }\
    }\
/*    MMLOGV("releasing pp: %p\n", _sb);*/\
    free(_sb);\
/*    MMLOGV("releasing pp: %p OVER\n", _sb);*/\
    (_sb) = NULL;\
}while(0)

#define FAIL_RET() do {\
    RELEASE_SCREEN_BUF(thesb, thegf->SHeight);\
    DGifCloseFile(thegf, &err);\
    return MMBufferSP((MMBuffer*)NULL);\
}while(0)

MMBufferSP GifDecoder::decode(/* [in] */const char * uri,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    if (!uri || scaleDenom == 0 || scaleNum == 0) {
        MMLOGE("invalid param: srcBuf: %p, scaleNum: %u, scaleDenom: %u\n", uri, scaleNum, scaleDenom);
        return MMBufferSP((MMBuffer*)NULL);
    }
    MMLOGV("uri: %s, scaleNum: %u, scaleDenom: %u, outFormat: %d\n", uri, scaleNum, scaleDenom);

    int err;
    GifFileType * thegf = DGifOpenFileName(uri, &err);
    if ( !thegf ) {
        MMLOGE("failed to open file %s\n", uri);
        return MMBufferSP((MMBuffer*)NULL);
    }

    int i;
    int j;
    GifRecordType thert;
    int thesz;
    int theR;
    int thec;
    GifRowType *thesb;
    int theec;
    GifByteType *theet;
    int theilo[] = {
        0,
        4,
        2,
        1 };
    ColorMapObject *thecm;
    int theilj[] = {
        8,
        8,
        4,
        2 };
    int thein = 0;

    if ((thesb = (GifRowType *)
        malloc(thegf->SHeight * sizeof(GifRowType))) == NULL) {
        MMLOGE("Failed to allocate thesb.");
        FAIL_RET();
    }
    memset(thesb, 0, thegf->SHeight * sizeof(GifRowType));

    thesz = thegf->SWidth * sizeof(GifPixelType);
    if ((thesb[0] = (GifRowType) malloc(thesz)) == NULL)  {
        MMLOGE("Failed to allocate ScreenBuffer0.");
        FAIL_RET();
    }

    MMLOGV("w: %d, h: %d, bkcolor: 0x%x",
        thegf->SWidth,
        thegf->SHeight,
        thegf->SBackGroundColor);

    for (i = 0; i < thegf->SWidth; i++)
        thesb[0][i] = thegf->SBackGroundColor;

    for (i = 1; i < thegf->SHeight; i++) {
        if ((thesb[i] = (GifRowType) malloc(thesz)) == NULL) {
            MMLOGE("failed to allocate row\n");
            FAIL_RET();
        }

        memcpy(thesb[i], thesb[0], thesz);
    }

    do {
        if (DGifGetRecordType(thegf, &thert) == GIF_ERROR) {
            MMLOGE("failed to DGifGetRecordType\n");
            FAIL_RET();
        }
        switch (thert) {
        case IMAGE_DESC_RECORD_TYPE:
            MMLOGV("rectype: image desc\n");
            if (DGifGetImageDesc(thegf) == GIF_ERROR) {
                MMLOGE("failed to DGifGetImageDesc: %s\n", GifErrorString(thegf->Error));
                FAIL_RET();
            }
            theR = thegf->Image.Top;
            thec = thegf->Image.Left;
            width = thegf->Image.Width;
            height = thegf->Image.Height;
            MMLOGV("Image %d at (%d, %d) [%dx%d]:     ",
                ++thein, thec, theR, width, height);
            if (thegf->Image.Left + thegf->Image.Width > thegf->SWidth ||
                thegf->Image.Top + thegf->Image.Height > thegf->SHeight) {
                MMLOGE("Image %d is not confined to screen dimension, aborted.\n",thein);
                FAIL_RET();
            }
            if (thegf->Image.Interlace) {
                MMLOGV("Interlace\n");
                for (i = 0; i < 4; i++)
                    for (j = theR + theilo[i]; j < theR + (int)height; j += theilj[i]) {
                        if (DGifGetLine(thegf, &thesb[j][thec], (int)width) == GIF_ERROR) {
                            MMLOGE("failed to get line: %s\n", GifErrorString(thegf->Error));
                            FAIL_RET();
                        }
                    }
            } else {
                MMLOGV("not Interlace\n");
                for (i = 0; i < (int)height; i++) {
                    if (DGifGetLine(thegf, &thesb[theR++][thec], (int)width) == GIF_ERROR) {
                        MMLOGE("failed to get line: %s\n", GifErrorString(thegf->Error));
                        FAIL_RET();
                    }
                }
            }
            break;
        case EXTENSION_RECORD_TYPE:
            MMLOGV("rectype: extension\n");
            if (DGifGetExtension(thegf, &theec, &theet) == GIF_ERROR) {
                MMLOGE("failed to get DGifGetExtension: %s\n", GifErrorString(thegf->Error));
                FAIL_RET();
            }
            while (theet != NULL) {
                if (DGifGetExtensionNext(thegf, &theet) == GIF_ERROR) {
                    MMLOGE("failed to get DGifGetExtensionNext: %s\n", GifErrorString(thegf->Error));
                    FAIL_RET();
                }
            }
            break;
        case TERMINATE_RECORD_TYPE:
            MMLOGV("rectype: TERMINATE\n");
            break;
        default:
            MMLOGV("rectype: unrecognized\n");
            break;
        }
    } while (thert != TERMINATE_RECORD_TYPE);

    MMLOGV("setting color map\n");
    thecm = (thegf->Image.ColorMap
                            ? thegf->Image.ColorMap
                            : thegf->SColorMap);
    if (thecm == NULL) {
        MMLOGE("Gif Image does not have a colormap\n");
        FAIL_RET();
    }

    MMBufferSP buf = MMBuffer::create(thegf->SWidth * 3 * thegf->SHeight);
    if (!buf) {
        MMLOGE("no mem\n");
        FAIL_RET();
    }
    buf->setActualSize(buf->getActualSize());

    MMLOGV("to rgb\n");
    for (i = 0; i < thegf->SHeight; i++) {
        uint8_t *thebp;
        GifRowType thegr = thesb[i];
        for (j = 0, thebp = buf->getWritableBuffer() + i * thegf->SWidth * 3; j < thegf->SWidth; j++) {
            GifColorType * cm = &thecm->Colors[thegr[j]];
            *thebp++ = cm->Red;
            *thebp++ = cm->Green;
            *thebp++ = cm->Blue;
        }
    }

    RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
    DGifCloseFile(thegf, &err);

    MMBufferSP scaledBuf;
    if (scaleNum != scaleDenom) {
        MMLOGV("scale\n");
        uint32_t srcWidth = width;
        uint32_t srcHeight = height;
        if (RGB::zoom(buf,
                RGB::kFormatRGB24,
                srcWidth,
                srcHeight,
                scaleNum,
                scaleDenom,
                scaledBuf,
                width,
                height) != MM_ERROR_SUCCESS) {
            MMLOGE("failed to zoom\n");
            return MMBufferSP((MMBuffer*)NULL);
        }
    } else {
        scaledBuf = buf;
    }

    MMBufferSP convertedBuf;
    if (RGB::kFormatRGB24 != outFormat) {
        convertedBuf = RGB::convertFormat(RGB::kFormatRGB24,
                outFormat,
                width,
                height,
                scaledBuf);
    } else {
        convertedBuf = scaledBuf;
    }

    return convertedBuf;
}


}

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

#include "media_decrypt.h"
#include "media_keys.h"

namespace YUNOS_DRM {

MediaDecrypt::MediaDecrypt(MediaKeys *mediaKeys, std::vector<uint8_t> &initData)
    : mMediaKeys(mediaKeys),
      mInitData(initData) {

    if (mediaKeys)
        mKeySystem = mediaKeys->getName();
}

MediaDecrypt::~MediaDecrypt() {

}

bool MediaDecrypt::requireSecureDecoder(const char *mimeType) {

    // this could be override by sub-class
    return true;
}

bool MediaDecrypt::setMediaKeySession(SessionID &session) {

    if (mMediaKeys == NULL || (mMediaKeys->getSession(session) == NULL))
        return false;

    mSessionID = session;
    return true;
}

ssize_t MediaDecrypt::decrypt(
            bool secure,
            const uint8_t key[16],
            const uint8_t iv[16],
            Mode mode,
            const uint8_t *src,
            uint8_t *dest,
            std::string &errorStr) {


    return decrypt( secure,
            key, iv,
            mode, src,
            NULL, 0,
            dest, errorStr);
}

ssize_t MediaDecrypt::decrypt(
            bool secure,
            const uint8_t *src,
            uint8_t *dest,
            std::string &errorStr) {

    return decrypt(
            secure,
            NULL,
            NULL,
            ModeAESFIXED,
            src,
            dest,
            errorStr);
}

} // end of namespace YUNOS_DRM

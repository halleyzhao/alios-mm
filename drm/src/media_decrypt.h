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

#ifndef __media_decrypt_h
#define __media_decrypt_h

#include "media_key_type.h"
#include "media_key_utils.h"

#include <string>
#include <tr1/memory>

namespace YUNOS_DRM {

class MediaKeys;

class MediaDecrypt {

public:
    /**
     * Instantiate a MediaDecrypt object, decrypt drm data
     * @param mediaKeys: the media keys object which back the MediaDecrypt object
     * @param initData: Opaque initialization data specific to the key system, e.g. sessionID
     */
    MediaDecrypt(MediaKeys *mediaKeys, std::vector<uint8_t> &initData);
    virtual ~MediaDecrypt();

#if 0
    /**
     * Query if the decrypt scheme is supported
     * @param keySystem: the name of the key system
     */
    static bool isDecryptSchemeSupported(const char *keySystem) const;
#endif

    /**
     * Query if the decrypt scheme requires the use of a secure decoder to decode
     * data of given type of media data
     * @param mimeType: mime type of the media data
     */
    virtual bool requireSecureDecoder(const char *mimeType);

    /**
     * Associate a MediaKey session with this MediaDecrypt object. The MediaKey session
     * is used to securely load decryption keys for a crypto scheme. The crypto keys
     * loaded through the MediaKey session may be selected for use during the decryption
     * operation
     * @param session: MediaKey session to be Associated for media decryption
     */
    bool setMediaKeySession(SessionID &session);

    /**
     * Decrypt a media sample with identified key and iv. SubSample decryption is supported
     * which is described by subSamples parameter. Returns a non-negative result to indicate
     * the number of bytes written to the dest pointer, or a negative result to indicate an
     * error.
     * @param secure: secure media sample or not
     * @param key: a 16-bytes opaque key
     * @param iv: a 16-byte initialization vector
     * @param mode: decryption mode to decrypt the sample
     * @param src: pointer to input sample for decryption
     * @param subSamples: number of sub samples within the input sample
     * @param numSubSamples:
     *        the structure of a (at least partially) encrypted input sample is considered to be
     *        partitioned into "subSamples", each subSample starts with a (potentially empty) run
     *        of plain, unencrypted bytes followed by a (also potentially empty) run of encrypted
     *        bytes. This information encapsulates per-sample metadata as outlined in ISO/IEC FDIS
     *        23001-7:2011 "Common encryption in ISO base media file format files".
     * @param dest: pointer to output buffer
     * @param errorStr: error string if there is
     */
    virtual ssize_t decrypt(
            bool secure,
            const uint8_t key[16],
            const uint8_t iv[16],
            Mode mode,
            const uint8_t *src,
            const SubSample *subSamples,
            size_t numSubSamples,
            uint8_t *dest,
            std::string &errorStr) = 0;

    /**
     * Decrypt a media sample with identified key and iv.
     */
    ssize_t decrypt(
            bool secure,
            const uint8_t key[16],
            const uint8_t iv[16],
            Mode mode,
            const uint8_t *src,
            uint8_t *dest,
            std::string &errorStr);

    /**
     * Decrypt a media sample with fixed key and iv
     */
    ssize_t decrypt(
            bool secure,
            const uint8_t *src,
            uint8_t *dest,
            std::string &errorStr);

protected:
    /*
     * Create crypto plugin, e.g. for MediaDrm
     */
    //bool createPlugin(const char *keySystem, std::vector<uint8_t> &initData);
    //bool destroyPlugin();

    //void notifyResolution(uint32_t widht, uint32_t height);

    std::string mKeySystem;
    MediaKeys *mMediaKeys;
    std::vector<uint8_t> mInitData;
    SessionID mSessionID; //MediaKeySession ID

    DRM_DISALLOW_COPY(MediaDecrypt);
};

typedef std::tr1::shared_ptr<MediaDecrypt> MediaDecryptSP;

} // end of namespace YUNOS_DRM
#endif //__media_decrypt_h

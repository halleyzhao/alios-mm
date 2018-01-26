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

#ifndef media_key_factory_h
#define media_key_factory_h

#include "media_key_type.h"
#include "media_key_utils.h"

namespace YUNOS_DRM {

class MediaKeySession;
class MediaKeys;

typedef std::tr1::shared_ptr<MediaKeySession> MediaKeySessionSP;

class DrmSessionFactory {
public:
    static MediaKeySession* createSession(MediaKeys* mediaKeys, MediaKeySessionType type);

    struct DrmPluginCore {
        typedef MediaKeySession*(*CreateDrmSessionFunc)(MediaKeys*, MediaKeySessionType);
        typedef void (*ReleaseDrmSessionFunc)(MediaKeySession*);
        typedef bool (*IsTypeSupported)(const char *, const char *);

        CreateDrmSessionFunc mCreate;
        ReleaseDrmSessionFunc mRelease;
        IsTypeSupported mIsTypeSupported;

        void *mLibHandle;
        const char* mLibName;
        //std::list<MediaKeySession*> mSessions;
    };

private:

    DrmSessionFactory() {}
    virtual ~DrmSessionFactory(){}

    static void releaseSession(MediaKeySession *session);
    static void loadPlugin(const char* libName);
    //static MediaKeySessionSP loadComponent_l(const char* libName,
        //const char* mimeType);

    typedef std::tr1::shared_ptr<DrmPluginCore> DrmPluginCoreSP;

    static std::map<std::string, DrmPluginCoreSP> mDrmPluginCoreMap;

    static DrmLock mLock;
};

} // YUNOS_DRM namespace

#endif // media_key_factory.h

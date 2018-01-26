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

#include "media_keys.h"
#include "media_key_session.h"
#include "media_key_factory.h"

#include <string.h>
#include <dlfcn.h>

namespace YUNOS_DRM {

#define ENTER() INFO(">>>\n")
#define EXIT() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)


// Marlin PSSH system ID
#define MarlinPSSH {0x69,0xf9,0x08,0xaf,0x48,0x16,0x46,0xea,0x91,0x0c,0xcd,0x5d,0xcc,0xcb,0x0a,0x3a}

// Marlin MPD system ID
#define MarlinMPD {0x5E,0x62,0x9A,0xF5,0x38,0xDA,0x40,0x63,0x89,0x77,0x97,0xFF,0xBD,0x99,0x02,0xD4}

#define WVClearKey {0x10,0x77,0xEF,0xEC,0xC0,0xB2,0x4D,0x02,0xAC,0xE3,0x3C,0x1E,0x52,0xE2,0xFB,0x4B}
#define WidevineUUID {0xED,0xEF,0x8B,0xA9,0x79,0xD6,0x4A,0xCE,0xA3,0xC8,0x27,0xDC,0xD5,0x1D,0x21,0xED}

// playready PSSH/MPD system ID: 9a04f079-9840-4286-ab92-e65be0885f95

#define KeySystemStringLen 16

struct KeySystemItem {
    uint8_t mKeySystem[KeySystemStringLen];
    const char *mLibName;
};

static const KeySystemItem KeySystemToLibName[] = {
   { MarlinPSSH, "WasabiDrmPlugin"},
   { MarlinMPD, "WasabiDrmPlugin"},
   { WVClearKey, "WVDrmPlugin"},
   { WidevineUUID, "WVDrmPlugin"},
   { {0,}, NULL},
};

static const char* getLibName(const char* keySystem) {

    if (!keySystem)
        return NULL;

    int i = 0;

    while (KeySystemToLibName[i++].mLibName != NULL) {

        int j = 0;

        if (strlen(keySystem) < KeySystemStringLen)
            continue;

        for (; j < KeySystemStringLen; j++) {
            if (keySystem[j] != KeySystemToLibName[i].mKeySystem[j])
                break;
        }

        if (j == KeySystemStringLen)
            return KeySystemToLibName[i].mLibName;
    }

    return NULL;
}

#define DrmPluginPath "/usr/lib/"

DrmLock DrmSessionFactory::mLock;
std::map<std::string, DrmSessionFactory::DrmPluginCoreSP> DrmSessionFactory::mDrmPluginCoreMap;

#if 0
/*static*/ void* DrmSessionFactory::loadDrmModule(MediaKeys* mediaKeys) {

    DrmAutoLock lock(mLock);

    const char* libName;
    const char* keySystem = mediaKeys->getName();

    libName = getLibName(keySystem);
    if (!libName) {
        ERROR("doesn't support key system %s", keySystem ? keySystem : "none");
        return NULL;
    }

    INFO("load drm module: key system is %s", keySystem);

    if (mDrmPluginCoreMap.empty()) {
        loadPlugin(libName);
    }

    DrmPluginCoreSP core = mDrmPluginCoreMap[libName];

    if (!core || !core->mLoadDrmModule) {
        INFO("plugin %s has no loadDrmModule method, return NULL", libName);
        return NULL;
    }

    return core->mLoadDrmModule(mediaKeys);
}
#endif

/*static*/ MediaKeySession* DrmSessionFactory::createSession(MediaKeys* mediaKeys, MediaKeySessionType type) {
    ENTER();

    DrmAutoLock lock(mLock);

    const char* libName;
    const char* keySystem = mediaKeys->getName();

    libName = getLibName(keySystem);
    if (!libName) {
        ERROR("doesn't support key system %s", keySystem ? keySystem : "none");
        return NULL;
    }

    INFO("create drm session, session type is %d", type);

    if (mDrmPluginCoreMap.empty()) {
        loadPlugin(libName);
    }

    DrmPluginCoreSP core = mDrmPluginCoreMap[libName];

    if (!core || !core->mCreate) {
        ERROR("fail to get sym from plugin %s", libName);
        return NULL;
    }

    MediaKeySession* tmp = core->mCreate(mediaKeys, type);
    if (!tmp) {
        ERROR("fail to create session from plugin %s", libName);
        return NULL;
    }

    return tmp;
}

/*static*/ void DrmSessionFactory::releaseSession(MediaKeySession *session) {
    INFO("release session %p", session);
}

/*static*/ void DrmSessionFactory::loadPlugin(const char* libName) {

    std::map<std::string, DrmPluginCoreSP>::iterator it = mDrmPluginCoreMap.find(libName);
    if (it != mDrmPluginCoreMap.end() && it->second->mLibHandle) {
        INFO("%s is already loaded", libName);
        return;
    }

    DrmPluginCoreSP core(new DrmPluginCore);

    core->mLibName = libName;
    std::string name = DrmPluginPath;
    name.append("lib");
    name.append(libName);
    name.append(".so");

    INFO("dlopen file (%s) to load plugin", name.c_str());
    core->mLibHandle = dlopen(name.c_str(), RTLD_NOW);
    if (!core->mLibHandle) {
        ERROR("fail to dlopen lib %s", name.c_str());
        return;
    }

    //core->mLoadDrmModule = (DrmPluginCore::LoadDrmModule)dlsym(core->mLibHandle, "loadDrmModule");
    core->mCreate = (DrmPluginCore::CreateDrmSessionFunc)dlsym(core->mLibHandle, "createSession");
    core->mRelease = (DrmPluginCore::ReleaseDrmSessionFunc)dlsym(core->mLibHandle, "releaseSession");
    core->mIsTypeSupported = (DrmPluginCore::IsTypeSupported)dlsym(core->mLibHandle, "isTypeSupported");

    if (!core->mCreate || !core->mRelease || !core->mIsTypeSupported) {
        ERROR("fail to get symboles from plugin %s - %s", name.c_str(), libName);
        ERROR("dlerror is %s", dlerror());
        INFO("createSession func is %p", core->mCreate);
        INFO("releaseSession func is %p", core->mRelease);
        INFO("isTypeSupported func is %p", core->mIsTypeSupported);

        return;
    }

    mDrmPluginCoreMap[libName] = core;
}

} // YUNOS_DRM namespace

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

/*
config file:

<?xml version="1.0" encoding="utf-8"?>

<soundservice version="1.0">
    <group name="touch_sounds">
        <sound type="ST_KEY_CLICK" file="Effect_Tick.ogg"/>
        <sound type="ST_FOCUS_NAVIGATION_UP" file="Effect_Tick.ogg"/>
        <sound type="ST_FOCUS_NAVIGATION_DOWN" file="Effect_Tick.ogg"/>
        <sound type="ST_FOCUS_NAVIGATION_LEFT" file="Effect_Tick.ogg"/>
        <sound type="ST_FOCUS_NAVIGATION_RIGHT" file="Effect_Tick.ogg"/>
        <sound type="ST_KEYPRESS_STANDARD" file="KeypressStandard.ogg"/>
        <sound type="ST_KEYPRESS_SPACEBAR" file="KeypressSpacebar.ogg"/>
        <sound type="ST_KEYPRESS_DELETE" file="KeypressDelete.ogg"/>
        <sound type="ST_KEYPRESS_RETURN" file="KeypressReturn.ogg"/>
        <sound type="ST_KEYPRESS_INVALID" file="KeypressInvalid.ogg"/>
        <sound type="ST_SCREEN_LOCK" file="Lock.ogg"/>
        <sound type="ST_SCREEN_UNLOCK" file="Unlock.ogg"/>
    </group>
</soundservice>

*/

#include "expat.h"
#include "cras_types.h"
#include "SysSoundServiceAdaptor.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

using namespace yunos;

static const size_t MAX_CHANNELS = 5;
static const char * SOUND_CONFIG_FILE = "/etc/syssound.xml";
static const char * SOUND_MEDIA_DIR = "/usr/data/media/audio/ui/";
static const char * SOUND_TAG = "sound";
static const int SAMPLE_ID_NOT_LOADED = -1;
static const int SAMPLE_ID_LOAD_FAILED = -2;


DEFINE_LOGTAG(SysSoundServiceAdaptor);
DEFINE_LOGTAG(SysSoundServiceAdaptor::ConfigLoader);
DEFINE_LOGTAG(SysSoundServiceAdaptor::InstantAudioListener);


SysSoundServiceAdaptor::SysSoundServiceAdaptor(const yunos::SharedPtr<yunos::DService>& service,
                                            yunos::String /*serviceName*/,
                                            yunos::String /*pathName*/,
                                            yunos::String /*iface*/,
                                            void* /*arg*/)
                                : yunos::DAdaptor(service, yunos::String(ISysSound::pathName()), yunos::String(ISysSound::_interface())),
                                mListener(NULL),
                                mInstantAudio(NULL)
{
    MMLOGI("+\n");
    sem_init(&mSem, 0, 0);
    MMLOGI("-\n");
}

SysSoundServiceAdaptor::~SysSoundServiceAdaptor()
{
    MMLOGI("+\n");
    if(mInstantAudio){
        mInstantAudio->unloadAll();
        InstantAudio::destroy(mInstantAudio);
        mInstantAudio = NULL;
    }
    MM_RELEASE(mListener);
    sem_destroy(&mSem);
    MMLOGI("-\n");
}

bool SysSoundServiceAdaptor::handleMethodCall(const yunos::SharedPtr<yunos::DMessage>& msg)
{
    if (msg->methodName() == "play") {
        MMLOGV("play\n");
        int32_t type = msg->readInt32();
        double leftVolume = msg->readDouble();
        double rightVolume = msg->readDouble();
        play((SoundType)type, (float)leftVolume, (float)rightVolume);
    } else {
        MMLOGE("unknown call: %s\n", msg->methodName().c_str());
    }

    return true;
}

bool SysSoundServiceAdaptor::init()
{
    MMLOGI("+\n");
    if (!ConfigLoader::load(this)) {
        MMLOGE("failed to load config\n");
        return false;
    }

    mListener = new InstantAudioListener(this);
    if (!mListener) {
        MMLOGE("no mem\n");
        return false;
    }

    mInstantAudio = InstantAudio::create(MAX_CHANNELS);
    if (!mInstantAudio) {
        MM_RELEASE(mListener);
        return false;
    }

    mInstantAudio->setListener(mListener);

    for (int i = 0; i < ST_COUNT; ++i) {
        if (mSoundInfo[i].mSampleId != SAMPLE_ID_NOT_LOADED) {
            MMLOGV("%s(%d) loaded, ignore\n", mSoundInfo[i].mPath.c_str(), mSoundInfo[i].mSampleId);
            continue;
        }
        MMLOGI("loading %s\n", mSoundInfo[i].mPath.c_str());
        if (mInstantAudio->load(mSoundInfo[i].mPath.c_str()) != MM_ERROR_SUCCESS) {
            MMLOGE("failed to load %s\n", mSoundInfo[i].mPath.c_str());
            continue;
        }
        MMLOGV("waitting...\n");
        sem_wait(&mSem);
        MMLOGV("waitting...over\n");
    }

    MMLOGI("-\n");
    return true;
}

/*static*/ bool SysSoundServiceAdaptor::ConfigLoader::load(SysSoundServiceAdaptor * watcher)
{
    MMLOGI("+\n");
    MMASSERT(watcher != NULL);
    FILE *f = fopen(SOUND_CONFIG_FILE, "r");

    if (!f) {
        MMLOGE("failed to open %s.\n", SOUND_CONFIG_FILE);
        return false;
    }

    XML_Parser parser = XML_ParserCreate(NULL);
    if (parser == NULL) {
        MMLOGE("fail to create XML parser");
        fclose(f);
        return false;
    }

    XML_SetUserData(parser, watcher);
    XML_SetElementHandler(parser, startElementHandler, NULL);

    static const int PARSE_BUFF_SIZE = 512;
    while (1) {
        void * buf = XML_GetBuffer(parser, PARSE_BUFF_SIZE);
        if (!buf) {
            MMLOGE("failed to XML_GetBuffer()");
            fclose(f);
            return false;
        }

        int bytes_read = fread(buf, 1, PARSE_BUFF_SIZE, f);
        if (bytes_read <= 0) {
            MMLOGV("read over\n");
            break;
        }

        if (XML_ParseBuffer(parser, bytes_read, bytes_read == 0)
                != XML_STATUS_OK) {
            MMLOGE("failed to parse buffer\n");
            fclose(f);
            return false;
        }
    }

    fclose(f);
    MMLOGI("-\n");
    return true;
}

/*static*/ void SysSoundServiceAdaptor::ConfigLoader::startElementHandler(
        void * userData, const char *name, const char **attrs)
{
    if (strcmp(name, SOUND_TAG)) {
        MMLOGV("ignore name: %s\n", name);
        return;
    }

    size_t i = 0;
    const char * type = NULL;
    const char * file = NULL;
    while (attrs[i] != NULL && attrs[i + 1] != NULL) {
        if (!strcmp(attrs[i], "type")) {
            type = attrs[i + 1];
        } else if (!strcmp(attrs[i], "file")) {
            file = attrs[i + 1];
        }

        ++i;
    }

    if (!type || !file) {
        MMLOGE("invalid file\n");
        return;
    }

    SysSoundServiceAdaptor * watcher = static_cast<SysSoundServiceAdaptor*>(userData);
    MMASSERT(watcher != NULL);
    if (!watcher) {
        MMLOGE("invalid params\n");
        return;
    }

    MMLOGI("type: %s, file: %s\n", type, file);
    watcher->addSound(type, file);
}

SysSoundServiceAdaptor::InstantAudioListener::InstantAudioListener(SysSoundServiceAdaptor * watcher)
                                            : mWatcher(watcher)
{
}

SysSoundServiceAdaptor::InstantAudioListener::~InstantAudioListener()
{
}

void SysSoundServiceAdaptor::InstantAudioListener::onMessage(int msg, int param1, int param2, const MMParam *obj)
{
        MMLOGD("msg: %d, param1: %d, param2: %d, obj: %p", msg, param1, param2, obj);
        switch ( msg ) {
            case URI_SAMPLE_LOADED:
                {
                    const char * uri = obj->readCString();
                    MMLOGV("uri: %s\n", uri);
                    if (param1 != MM_ERROR_SUCCESS || param2 < 0) {
                        mWatcher->sampleLoaded(uri, SAMPLE_ID_LOAD_FAILED);
                    } else {
                        mWatcher->sampleLoaded(uri, param2);
                    }
                }
                break;
            default:
                break;
        }
}

SysSoundServiceAdaptor::SoundInfo::SoundInfo()
                            : mPath(""),
                            mSampleId(SAMPLE_ID_NOT_LOADED)
{
}

void SysSoundServiceAdaptor::addSound(const char * type, const char * file)
{
#define CHECK_TYPE(_type) do {\
    if (!strcmp(type, #_type)) {\
        mSoundInfo[_type].mPath = SOUND_MEDIA_DIR;\
        mSoundInfo[_type].mPath += file;\
        MMLOGI("type: %s, file: %s\n", #_type, mSoundInfo[_type].mPath.c_str());\
        return;\
    }\
}while(0)

    CHECK_TYPE(ST_KEY_CLICK);
    CHECK_TYPE(ST_FOCUS_NAVIGATION_UP);
    CHECK_TYPE(ST_FOCUS_NAVIGATION_DOWN);
    CHECK_TYPE(ST_FOCUS_NAVIGATION_LEFT);
    CHECK_TYPE(ST_FOCUS_NAVIGATION_RIGHT);
    CHECK_TYPE(ST_KEYPRESS_STANDARD);
    CHECK_TYPE(ST_KEYPRESS_SPACEBAR);
    CHECK_TYPE(ST_KEYPRESS_DELETE);
    CHECK_TYPE(ST_KEYPRESS_RETURN);
    CHECK_TYPE(ST_KEYPRESS_INVALID);
    CHECK_TYPE(ST_SCREEN_LOCK);
    CHECK_TYPE(ST_SCREEN_UNLOCK);

    MMLOGE("unknown type: %s, file: %s\n", type, file);
}

void SysSoundServiceAdaptor::sampleLoaded(const char * uri, int sampleId)
{
    MMLOGI("uri: %s, id: %d\n", uri, sampleId);
    SoundInfo * p = mSoundInfo;
    for (int i = 0; i < ST_COUNT; ++i, ++p) {
        if (!strcmp(p->mPath.c_str(), uri)) {
            MMLOGI("type: %d, uri: %s, sample id: %d\n", i, uri, sampleId);
            p->mSampleId = sampleId;
        }
    }
    sem_post(&mSem);
    MMLOGV("-\n");
}

mm_status_t SysSoundServiceAdaptor::play(SoundType soundType, float leftVolume, float rightVolume)
{
    MMLOGV("soundType: %d, left: %f, right: %f\n", soundType, leftVolume, rightVolume);
    if (soundType < 0 || soundType >= ST_COUNT) {
        MMLOGE("invalid sound type: %d\n", soundType);
        return -1;
    }
    InstantAudio::VolumeInfo v;
    v.left = leftVolume;
    v.right = rightVolume;
    return mInstantAudio->play(mSoundInfo[soundType].mSampleId,
        &v,
        0,
        1.0,
        1,
        CRAS_STREAM_TYPE_SYSTEM) >= 0 ? MM_ERROR_SUCCESS : MM_ERROR_OP_FAILED;
}

}

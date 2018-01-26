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

#ifndef __SysSoundServiceAdaptor_H
#define __SysSoundServiceAdaptor_H

#include <semaphore.h>

#include <dbus/DAdaptor.h>
#include <dbus/DMessage.h>
#include <dbus/DService.h>

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/ISysSound.h>
#include <multimedia/instantaudio.h>

namespace YUNOS_MM {

class SysSoundServiceAdaptor : public yunos::DAdaptor, public ISysSound {
public:
    SysSoundServiceAdaptor(const yunos::SharedPtr<yunos::DService>& service,
        yunos::String, yunos::String, yunos::String, void*);
    virtual ~SysSoundServiceAdaptor();

public:
    virtual bool handleMethodCall(const yunos::SharedPtr<yunos::DMessage>& msg);

    bool init();
    virtual mm_status_t play(SoundType soundType, float leftVolume, float rightVolume);

protected:
    struct SoundInfo {
        SoundInfo();

        std::string mPath;
        int mSampleId;
    };

    class ConfigLoader {
    public:
        static bool load(SysSoundServiceAdaptor * watcher);
        static void startElementHandler(
                void * userData, const char *name, const char **attrs);

        DECLARE_LOGTAG();
        MM_DISALLOW_COPY(ConfigLoader);
    };

    class InstantAudioListener : public InstantAudio::Listener {
    public:
        InstantAudioListener(SysSoundServiceAdaptor * watcher);
        virtual ~InstantAudioListener();

    public:
        virtual void onMessage(int msg, int param1, int param2, const MMParam *obj);

    private:
        SysSoundServiceAdaptor * mWatcher;
        DECLARE_LOGTAG();
        MM_DISALLOW_COPY(InstantAudioListener);
    };

    void addSound(const char * type, const char * file);
    void sampleLoaded(const char * uri, int sampleId);

private:
    InstantAudioListener * mListener;
    InstantAudio * mInstantAudio;
    SoundInfo mSoundInfo[ST_COUNT];
    sem_t mSem;

    DECLARE_LOGTAG();
    MM_DISALLOW_COPY(SysSoundServiceAdaptor);
};


}

#endif

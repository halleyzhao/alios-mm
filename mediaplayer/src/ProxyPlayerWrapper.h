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

#ifndef __ProxyPlayerWrapper_H
#define __ProxyPlayerWrapper_H

#include <multimedia/mediaplayer.h>

namespace YUNOS_MM {

class MediaPlayerClient;
class ProxyPlayerWrapper : public MediaPlayer {
public:
    ProxyPlayerWrapper(int playType = 0, const void * userDefinedData = NULL);
    ~ProxyPlayerWrapper();

public:
    virtual mm_status_t setListener(Listener * listener);
    virtual mm_status_t setDataSource(const char * uri,
                            const std::map<std::string, std::string> * headers = NULL);
    virtual mm_status_t setDataSource(int fd, int64_t offset, int64_t length);
    virtual mm_status_t setDataSource(const unsigned char * mem, size_t size);
    virtual mm_status_t setSubtitleSource(const char* uri);
    virtual mm_status_t setDisplayName(const char* name);
    virtual mm_status_t setVideoDisplay(void * handle);
    virtual mm_status_t setVideoSurfaceTexture(void * handle);
    virtual mm_status_t prepare();
    virtual mm_status_t prepareAsync();
    virtual mm_status_t setVolume(const VolumeInfo & volume);
    virtual mm_status_t getVolume(VolumeInfo * volume) const;
    virtual mm_status_t setMute(bool mute);
    virtual mm_status_t getMute(bool * mute) const;
    virtual mm_status_t start();
    virtual mm_status_t stop();
    virtual mm_status_t pause();
    virtual mm_status_t seek(int msec);
    virtual mm_status_t reset();
    virtual bool isPlaying() const;
    virtual mm_status_t getVideoSize(int *width, int * height) const;
    virtual mm_status_t getCurrentPosition(int * msec) const;
    virtual mm_status_t getDuration(int * msec) const;
    virtual mm_status_t setAudioStreamType(as_type_t type);
    virtual mm_status_t getAudioStreamType(as_type_t *type);
    virtual mm_status_t setLoop(bool loop);
    virtual bool isLooping() const;
    // A language code in either way of ISO-639-1 or ISO-639-2. When the language is unknown or could not be determined, MM_ERROR_UNSUPPORTED will returned
    virtual mm_status_t setParameter(const MediaMetaSP & meta);
    virtual mm_status_t getParameter(MediaMetaSP & meta);
    virtual mm_status_t invoke(const MMParam * request, MMParam * reply);
    virtual mm_status_t enableExternalSubtitleSupport(bool enable);

    virtual mm_status_t captureVideo();

private:
    bool mPlayType;
    MediaPlayerClient * mPlayer;

    MM_DISALLOW_COPY(ProxyPlayerWrapper)
    DECLARE_LOGTAG()
};

}

#endif // __ProxyPlayerWrapper_H

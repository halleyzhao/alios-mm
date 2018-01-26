/*
 * Copyright © 2017 Intel Corporation. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WOOGEEN_BASE_VIDEODECODERINTERFACE_H_
#define WOOGEEN_BASE_VIDEODECODERINTERFACE_H_

#include <memory>
#include "mediaformat.h"
#include "videoframebase.h"

namespace woogeen {
namespace base {
/**
 @brief VideoDecoderInterface allows defining a customized video processing
 instance to decode and render a specified stream it is attached to.
 @detail Application that wants to define their own decoding and rendering
 logic should implement this interface. When instantiating your
 videosinkinterface, you can pass in a surface handle in the ctor so when
 OnFrame() is called on your instance, this surface can be used.
*/
class VideoDecoderInterface {
 public:
  /**
   @brief Destructor
   */
  virtual ~VideoDecoderInterface() {}
  /**
   @brief This function initializes the customized video decoder
   @param video_codec Video codec of the encoded video stream
   @return true if successful or false if failed
   */
  virtual bool InitDecodeContext(MediaCodec::VideoCodec video_codec) = 0;

  /**
   @brief This function releases the customized video decoder
   @return true if successful or false if failed
   */
  virtual bool Release() = 0;

  /**
   @return true if successfully decode the frame, false otherwise.
   @details Decoder should release the frame by @saReleaseVieoEncodeFrame().
    Decoder should call ReleaseVieoEncodeFrame() as early as possible in order that
    the frame can be recycled to webrtc stack for future decoding.
    usually, Decoder may keeps ~3 frames from webrtc-stack.
   @param frame the video frame passed to the videosinkinterface instance
  */
  virtual bool OnFrame(VideoFrame* frame) = 0;
};

}
}

#endif // WOOGEEN_BASE_VIDEODECODERINTERFACE_H_

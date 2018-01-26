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

#ifndef WOOGEEN_BASE_VIDEOFRAMEBASE_H_
#define WOOGEEN_BASE_VIDEOFRAMEBASE_H_

#include <stdint.h>

namespace woogeen {
namespace base {

class VideoFrame {
 public:
  /** There are producer and consumer role of VideoFrame:
     - producer is Encoder for local stream and webrtc-stack for remote stream.
     - consumer is webrtc-stack for local stream and Decoder for remote stream.
     - producer takes care of the whole life cycle of VideoFrame
       . it defines sub-clas of VideoFrame
       . it manages the real buffer and calls SetFrameInfo() to init VideoFrame
       . it implements CleanUp() to release/recycle the real buffer of VideoFrame
     - consumer uses the buffer directly without copy; consumer should release VideoFrame
       by ReleaseVieoEncodeFrame() after use, as early as possible.
   */
  VideoFrame()
    : data_(NULL)
    , data_size_(0)
    , time_stamp_(-1)
    , flags_(0)
    { }

  /**
   @brief Called by video frame producer to set video frame information.
   @param data pointer of compressed video data.
   @param data_size size of compressed video data
   @timestamp timestamp of the video frame
   @return true on success.
  */
  bool SetFrameInfo(uint8_t* data, size_t data_size, int64_t time_stamp) {
    data_ = data;
    data_size_ = data_size;
    time_stamp_ = time_stamp;
    return true;
  }

  /**
   @brief Called by consumer. retrieve video frame information of compressed data
   @param data to be filled with video frame buffer address
   @param data_size to be filled with video frame size
   @param timestamp to be filled with time stamp of vidoe frame
   @return true on success
  */
  bool GetFrameInfo(uint8_t* &data, size_t &data_size, int64_t &time_stamp) {
    data = data_;
    data_size = data_size_;
    time_stamp = time_stamp_;
    return true;
  }


  enum VideoFrameFlag {
    VFF_KeyFrame   = 1,        // IDR frame
    VFF_NoneRef    = 1 << 1,   // can be discarded w/o impact to other frame
    VFF_EOS        = 1 << 2,   // the final video frame
  };

  /**
   @brief set frame flag
   @param new flag to set
  */
  void SetFlag(VideoFrameFlag flag) { flags_ |= flag; }

  /**
   @brief check whether the flag is set
   @param new flag to set
  */
  bool IsFlagSet(VideoFrameFlag flag) { return flags_ & flag; }

  /**
   @brief clear frame flag
   @param flag the flag bit to clear
  */
  void ClearFlag(VideoFrameFlag flag) { flags_ &= !flag; }

  /**
   @brief clear all frame flags
  */
  void ClearFlags() { flags_ = 0; }

  /**
   @brief cleanup resources allocated by sub-class, it must be called before destruction.
   @detail base class doesn't allocate any resource/memory, it is time for derived class
    to cleanup/recycle resource/memory before destruction.
    A pure virtual func ensures the base class not be used by mistake, since the base class isn't useful enough.
   @return usually true on success.
  */
  virtual bool CleanUp() = 0;
  static void ReleaseVieoFrame(VideoFrame* frame) {
      if (!frame)
          return;
      frame->CleanUp();
      delete frame;
  }

  /// make sure VideoFrame can't be freed directly, but by ReleaseVideoFrame()
 protected:
  virtual ~VideoFrame() { }

 private:
  uint8_t* data_;
  size_t data_size_;              /*data size within the buffer. For simplicity, always start from offset 0*/
  int64_t time_stamp_;
  uint32_t flags_;
  int32_t padding[11];
};

}
}

#endif  // WOOGEEN_BASE_VIDEOFRAMEBASE_H_

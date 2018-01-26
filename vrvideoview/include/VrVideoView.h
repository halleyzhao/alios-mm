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

#ifndef __vr_video_view_h
#define __vr_video_view_h

namespace yunos {

namespace yvr {

class VrViewController;

class VrVideoView {

public:

    /*
     * Describe how panoramic video get mapped to rect
     */
    enum PanoramicProjection {
       /*
        * Normal video, not panoramic
        */
       NONE,
       /*
        * spherical 360 degree video/image is stored in equirectangular-panoramic format
        * See https://en.wikipedia.org/wiki/Equirectangular_projection
        */
       EQUIRECTANGULAR,
       /*
        * 360 degree video/image is stored in cube_map format
        * See https://en.wikipedia.org/wiki/Cube_mapping
        */
       CUBEMAP
    };

    enum StereoMode {
       /*
        * Non-stereo with single view
        */
       MONOSCOPIC,
       /*
        * Stereo video with two views for left and right eyes respectively
        * The left half of picture is the view for left eye
        * The right half of picture is the view for right eyeright
        */
       STEREO_LEFT_RIGHT,
       /*
        * Stereo video with two views for left and right eyes respectively
        * The top half of picture is the view for left eye
        * The bottom of picture is the view for right eye
        */
       STEREO_TOP_BOTTOM
    };

    /*
     * Video presentation mode in app view
     */
    enum DisplayMode {
       MONO,
       STEREO
    };

    static VrVideoView* createVrView();
    /*
     * Initialize vr video view
     * Prepare everything to start rendering vr video or image
     */
    virtual bool init() = 0;

    /*
     * Create surface texture to receive player's video output
     */
    virtual void* createSurface() = 0;

    /*
     * Send surface texture to VrView which receives player's video output
     */
    virtual bool setSurfaceTexture(void *st) = 0;

    /*
     * Run player/codec in service, create BQ and return BQProducer fd
     */
    virtual int createProducerFd() = 0;

    /*
     * Set the render target for the view
     */
    virtual bool setDisplayTarget(void *displayTarget, int x, int y, int w, int h) = 0;

    /*
     * Set the render target for the view
     */
    virtual bool setDisplayTarget(const char *surfaceName, int x, int y, int w, int h) = 0;

    /*
     * set rotation as we play VR video is in landscape mode
     */
    virtual bool setVideoRotation(int degree) = 0;

    /*
     * Select pano projection mode of the content for the view
     */
    virtual bool setProjectionMode(PanoramicProjection mode) = 0;
    /*
     * Get pano projection mode of the content for the view
     */
    virtual void getProjectionMode(PanoramicProjection &mode) = 0;
    /*
     * Select stereo mode of the content for the view
     */
    virtual bool setStereoMode(StereoMode mode) = 0;
    /*
     * Get stereo mode of the content for the view
     */
    virtual void getStereoMode(StereoMode &mode) = 0;
    /*
     * Select the presention display mode for the view
     */
    virtual bool setDisplayMode(DisplayMode mode) = 0;
    /*
     * Get the presention display mode for the view
     */
    virtual void getDisplayMode(DisplayMode &mode) = 0;
    /*
     * Get the VR View controller
     * By defalut VrView update controller automatically,
     * but after this call VrView stop do that and let app
     * client to take control
     * After VrVideoView destroy, the pointer is no longer valid
     */
    virtual void getVrViewController(VrViewController** controller) = 0;

    virtual ~VrVideoView();

protected:
    VrVideoView();

private:
    VrVideoView(const VrVideoView&);
    VrVideoView& operator=(const VrVideoView&);
};

}; // end of namespace yvr
}; // end of namespace yunos

#endif // __vr_video_view_h

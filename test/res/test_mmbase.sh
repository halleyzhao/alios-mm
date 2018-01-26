#!/system/bin/sh
exec_func() {
    my_cmd=$1
    echo "### run command:  ${my_cmd}"
    date
    ${my_cmd}
    result=$?
    if [ "${result}" != "0" ]; then
        echo "*** failed: ${my_cmd}"
    fi
}

echo "Test Multimedia-base:"
exec_func "mmdebug-test"
exec_func "mmparam-test"
exec_func "mmthread-test"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -p 5"
exec_func "avdemuxer-test"
exec_func "avmuxer-test  /usr/bin/ut/res/video/test.mp4"
exec_func "clock-test"
exec_func "buffer-test /usr/bin/ut/res/video/test.mp4"
exec_func "meta-test /usr/bin/ut/res/audio/sp.mp3"
exec_func "monitor-test 50"
exec_func "vffmpeg-dtest -f /usr/bin/ut/res/video/test.mp4 -o /data/test -w 720 -h 480"
exec_func "vffmpeg-etest -i file:///usr/bin/ut/res/video/yuv420_320x240.yuv -o /data/test -t 3"
exec_func "video-sink-test"
exec_func "cow-audio-test /usr/bin/ut/res/audio/sp.mp3"
exec_func "cowrecorder-test -m a -p -t 5"
exec_func "factory-test"
platform=`getprop ro.yunos.platform.name`
if [ "$platform"x = "phone"x ]; then
exec_func "mediacodec-test -f /usr/bin/ut/res/video/trailer_short.mp4"
exec_func "mediacodec-record-test  /data/record.mp4 -s 1280x720"
fi

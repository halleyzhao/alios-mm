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

echo "Test Multimedia-cow:"
chmod 755 /usr/bin/cowplayer_test

exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -t 10"
exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -m -s 1280x720 -l 1 -p 3 -g 3 -c 2"
exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 0 -c 2 -w 0 -n 1"
exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 0 -c 2 -w 1 -n 1 -p 5"
exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 1 -c 2 -w 2 -n 2 -p 5"
exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 1 -c 2 -w 3 -n 2 -p 5"
exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 2 -c 2 -w 4 -n 2 -p 5"
exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 2 -c 2 -w 5 -n 2 -p 5"
exec_func "cowplayer_test -a /usr/bin/ut/res/video/test.mp4 -v -p 5"
exec_func "cowplayer_test -a /usr/bin/ut/res/audio/sp.mp3 -t 10"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -x -p 5"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -x -d /usr/bin/ut/res/video/test03.srt -p 35"
exec_func "cowplayer-test -a /usr/bin/ut/res/audio/sp.mp3 -x -p 5"
exec_func "cowaudioplayer-test -a /usr/bin/ut/res/audio/sp.mp3 -t 10"
exec_func "cowaudioplayer-test -a /usr/bin/ut/res/audio/sp.mp3 -l -c 2"
exec_func "cowaudioplayer-test -a /usr/bin/ut/res/audio/yjm.mp3 -l -c 2 -o"
exec_func "cow-record-test"
exec_func "cowrecorder-test -m v -l -c 2"
exec_func "cowrecorder-test -m v -i auto://  -l -c 2"
exec_func "cowrecorder-test -m v -a /data/cow_recorder.mp4 -l -c 2"
exec_func "cowrecorder-test -m v -f NV12 -l -c 2"
exec_func "cowrecorder-test -m a -p -t 5"
exec_func "avdemuxer-test"
exec_func "avmuxer-test  /usr/bin/ut/res/video/test.mp4"
exec_func "clock-test"
exec_func "buffer-test /usr/bin/ut/res/video/test.mp4"
exec_func "meta-test /usr/bin/ut/res/audio/sp.mp3"
exec_func "monitor-test 50"
exec_func "vffmpeg-dtest  -f /usr/bin/ut/res/video/test.mp4 -o /data/test -w 720 -h 480"
exec_func "vffmpeg-etest -i file:///usr/bin/ut/res/video/yuv420_320x240.yuv -o /data/test -t 3"
exec_func "video-sink-test"
exec_func "cow-audio-test /usr/bin/ut/res/audio/sp.mp3"
exec_func "factory-test"

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

echo "Test Multimedia-mediaplayer:"
exec_func "mediaplayer-test"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -t 10"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -t 10"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -m -s 1280x720 -l 1 -p 3 -g 3 -c 2"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 0 -c 2 -w 0 -n 1"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 0 -c 2 -w 1 -n 1 -p 5"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 1 -c 2 -w 2 -n 2 -p 5"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 1 -c 2 -w 3 -n 2 -p 5"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 2 -c 2 -w 4 -n 2 -p 5"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -t 10 -l 2 -c 2 -w 5 -n 2 -p 5"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -v -p 5"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -x -p 5"
exec_func "cowplayer-test -a /usr/bin/ut/res/audio/sp.mp3 -x -p 5"
exec_func "cowaudioplayer-test -a /usr/bin/ut/res/audio/sp.mp3 -t 10"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -x -d /usr/bin/ut/res/video/test03.srt -p 35"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -x -d /usr/bin/ut/res/video/test04.txt -p 35"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -x -d /usr/bin/ut/res/video/test05.smi -p 35"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -x -d /usr/bin/ut/res/video/test06.ssa -p 35"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4 -x -d /usr/bin/ut/res/video/test07.ass -p 35"


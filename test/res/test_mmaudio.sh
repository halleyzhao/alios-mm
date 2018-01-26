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

echo "Test Multimedia-audio:"
exec_func "audiodecode-test  /usr/bin/ut/res/audio/sp.mp3"
exec_func "pcmplayer-test  /usr/bin/ut/res/audio/audiocompat/pcms/__[11025_S16LE_2].pcm 1 44100 2"
exec_func "pcmrecord-test"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4"
exec_func "cowaudioplayer-test -a /usr/bin/ut/res/audio/sp.mp3 -t 10"
exec_func "instantaudio-test"

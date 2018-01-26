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

echo "Test Dash Play:"
exec_func "mpe-test"
exec_func "cowplayer-test -x -a http://10.97.176.52:4567/yichen.lr/20161220_drm/xiong_480/xiong.mpd -p 5"
exec_func "cowplayer-test -x -a http://10.97.176.52:4567/yichen.lr/20161220_drm/xiong_480/xiong.mpd -p 5 -t 10"

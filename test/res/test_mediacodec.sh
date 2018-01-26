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

echo "Test Multimedia-mediacodec:"
platform=`getprop ro.yunos.platform.name`
if [ "$platform"x = "phone"x ]; then
exec_func "mediacodec-test -f /usr/bin/ut/res/video/trailer_short.mp4"
exec_func "mediacodec-test -f /usr/bin/ut/res/video/trailer_short.mp4 -t"
exec_func "mediacodec-test -f /usr/bin/ut/res/video/trailer_short.mp4 -b"
exec_func "mediacodec-test -f /usr/bin/ut/res/video/trailer_short.mp4 -x 1 -y 1 -h 500 -w 500 -a 255 -c 100 -r"
exec_func "mediacodec-record-test  /data/record.mp4 -s 1280x720"
exec_func "cowplayer-test -a /usr/bin/ut/res/video/test.mp4"
exec_func "cowplayer-test -w 4 -a /usr/bin/ut/res/video/test.mp4"
exec_func "cowplayer-test -w 5 -a /usr/bin/ut/res/video/test.mp4"
exec_func "cowrecorder-test -m v -l -c 2"
exec_func "cow-record-test"
exec_func "cowrecorder-test -m v -l -c 2"
exec_func "cowrecorder-test -m v -i auto://  -l -c 2"
setprop mm.mc.force.dump.input  1
setprop mm.mc.force.dump.output  1
setprop mm.mst.check.gpu.sync  1
exec_func "mediacodec-test -f /usr/bin/ut/res/video/trailer_short.mp4"
setprop mm.mc.force.dump.input  0
setprop mm.mc.force.dump.output  0
setprop mm.mst.check.gpu.sync  0
else
echo "skip mediacodec-test on tablet"
fi

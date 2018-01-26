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

echo "Test PlaybackChannel:"
chmod 755 /usr/bin/pbchannel-test
chmod 755 /usr/bin/pbremoteipc-test
chmod 755 /usr/bin/pbchannelmanager-test

exec_func "pbchannel-test"
exec_func "pbchannel-test -m"
exec_func "pbchannel-test -m -p"
exec_func "pbchannel-test -p"
exec_func "pbchannelmanager-test"

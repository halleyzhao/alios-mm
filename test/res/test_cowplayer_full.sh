#!/system/bin/sh
chmod 755 /usr/bin/cowplayer-test

file="/usr/bin/ut/res/video/test.mp4"
echo "The test file is: $file"
count=2
echo "The loop count value is: $count"
players=1
while [ $players -lt 3 ];
do
  loop=-1;
  while [ $loop -lt 3 ];
  do
    windowtype=0;
    while [ $windowtype -lt 6 ];
    do
      if [ $loop -lt 0 ];then
        echo "Do Testcase: cowplayer-test -a $file -t 10 -w $windowtype -n $players";
        cowplayer-test -a $file -t 10 -w $windowtype -n $players
      else
        echo "Do Testcase: cowplayer-test -a $file -t 10 -l $loop -c $count -w $windowtype -n $players";
        cowplayer-test -a $file -t 10 -l $loop -c $count -w $windowtype -n $players
      fi
      windowtype=$(($windowtype+1));
    done;
    loop=$(($loop+1));
  done;
  players=$(($players+1));
done;

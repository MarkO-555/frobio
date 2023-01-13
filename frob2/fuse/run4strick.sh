#!/bin/bash
#
# This script is how strick runs his emulator.
# This certainly won't work for you unless modified.

set -ex

WHAT=${WHAT:-cocosdc}
WHAT=${WHAT:-80d}

cd $(dirname $0)
FUSE=$(pwd)         # Fuse Dir, absolute
FM=$FUSE/modules    # Fuse Modules, absolute

GH=$HOME/go/src/github.com
T=$GH/strickyak/doing_os9/gomar/drive/disk2

(
  cd modules/
  make -B LWASM=lwasm-orig
)
(
  cd $GH/strickyak/doing_os9/gomar/borges/
  go install borges.go
)

$HOME/go/bin/borges -outdir "$GH/strickyak/doing_os9/gomar/borges/" -glob '*.os9' .


(
  cd $GH/n6il/nitros9/level2/coco3 
  export NITROS9DIR=$GH/n6il/nitros9 
  make
  make -C bootfiles clean
  make -C bootfiles
  cat $FM/fuseman.os9 $FM/fuser.os9 $FM/fuse.os9 >> bootfiles/bootfile_$WHAT
  rm -f NOS9_6809_L2_v030300_coco3_$WHAT.dsk
  # This `make` depends on *not* re-making bootfiles.
  make  NOS9_6809_L2_v030300_coco3_$WHAT.dsk
  cp -v NOS9_6809_L2_v030300_coco3_$WHAT.dsk $T
)

(
  cd daemons/demos
  make -B
  for x in *.os9
  do
    y=$(basename $x .os9)
    os9 copy -r $x $T,CMDS/$y
    os9 attr -per $T,CMDS/$y
  done
)
(
  cd ..
  make -B
  for x in [fx].*.os9
  do
    y=$(basename $x .os9)
    os9 copy -r $x $T,CMDS/$y
    os9 attr -per $T,CMDS/$y
  done
  for x in [fx].*.shell
  do
    y=$(basename $x .shell)
    os9 copy -r -l $x $T,CMDS/$y
    os9 attr -per $T,CMDS/$y
  done
)

os9 copy -r -l /dev/stdin $GH/strickyak/doing_os9/gomar/drive/disk2,/s.5 <<\HERE 
-t
-p
-x
* fuse.twice >>>/w1 &
fuse.ramfile >>>/w1 &
sleep 10
* date > /fuse/twice/foo
date > /fuse/ramfile/short
date -t > /fuse/ramfile/long
dir > /fuse/ramfile/dir
echo ========
* list /fuse/twice/foo
* echo ========
sleep 5
list /fuse/ramfile/short
echo ========
sleep 5
list /fuse/ramfile/long
echo ========
sleep 5
list /fuse/ramfile/dir
echo ========
HERE

echo 'echo Nando' | os9 copy -r -l /dev/stdin $GH/strickyak/doing_os9/gomar/drive/disk2,/startup

(
  cd $GH/strickyak/doing_os9/gomar 

  case $WHAT in
    80d )
      go run -x --tags=coco3,level2 gomar.go -boot drive/boot2coco3 -disk drive/disk2 2>/dev/null
      ;;
    cocosdc )
      scp $T root@yak.net:/tmp/
      ;;
  esac

  # go run -x --tags=coco3,level2,trace gomar.go -boot drive/boot2coco3 -disk drive/disk2 --borges "$GH/strickyak/doing_os9/gomar/borges/" --trigger_os9='(?i:fork.*file=.sleep)' 2>/tmp/_
)

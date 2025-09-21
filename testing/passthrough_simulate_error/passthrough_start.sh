#!/usr/bin/env bash
set -u

mnt=~/mnt/simulate_error
mkdir -p $mnt
set -x
! mountpoint $mnt &&  umount $mnt
mountpoint  $mnt |grep 'connected' &&  sudo umount $mnt
tmux rename-window   passthrough
~/compiled/passthrough_simulate_error -f $mnt
ls $mnt

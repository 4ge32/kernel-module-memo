#!/bin/sh

mod=sample

case "$1" in
  run)
    sudo insmod $mod.ko
    ;;
  clean)
    sudo rmmod $mod
    ;;
  *)
    sudo insmod $mod.ko
    ;;
esac
exit 0

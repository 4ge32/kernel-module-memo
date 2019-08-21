#!/bin/sh

mod=sample
if [ $# -eq 2 ]; then
  mod=$2
fi

case "$1" in
  run)
    sudo insmod $mod.ko
    ;;
  clean)
    sudo rmmod $mod
    ;;
  *)
    echo "<Usage ./run.sh run/clean module_name>"
    ;;
esac
exit 0

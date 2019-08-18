#!/bin/sh

if [ $# -ne 1 ]; then
    echo "input dir name"
      exit 1
fi

dirname=$1

if [ ! -e $dirname ]; then
  mkdir -v $dirname
fi

if [ ! -e $dirname/Makefile ]; then
  cp -v Makefile $dirname/
fi

if [ ! -e $dirname/sample.c ]; then
  cp -v sample.c $dirname/
fi

if [ ! -L $dirname/sample.c ]; then
  ln -sf ../run.sh $dirname/run.sh
fi

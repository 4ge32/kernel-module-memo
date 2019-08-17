#!/bin/sh

if [ $# -ne 1 ]; then
    echo "input dir name"
      exit 1
fi

dirname=$1
mkdir -v $dirname
cp -v Makefile $dirname/
cp -v sample.c $dirname/
ln -sf ../run.sh $dirname/run.sh

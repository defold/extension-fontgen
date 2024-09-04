#!/usr/bin/env bash

DIR=$(dirname "$0")
TARGET=${DIR}/generator

clang -I${DIR}/../fontext/src ${DIR}/main.c -o ${TARGET}
echo "Wrote ${TARGET}"
echo "Run with: ${TARGET} C 14 ./assets/fonts/vera_mo_bd.ttf"

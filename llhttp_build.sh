#!/bin/bash

echo "hello?"
echo ${CFLAGS}

git clone https://github.com/nodejs/llhttp --depth=1
cd llhttp
npm ci
make clean
make build/libllhttp.a

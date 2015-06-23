#!/bin/bash
set -e

platform=$(uname -s | sed "y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/")

if [[ $(uname -s) != 'Linux' ]]; then
    exit 0
fi

npm run clean
npm install es6-shim
npm install request

# borrowed from node-sqlite3 scripts/build_against_node.sh

sudo apt-get update
sudo apt-get -y install gcc-multilib g++-multilib
# node v0.8 and above provide pre-built 32 bit and 64 bit binaries
# so here we use the 32 bit ones to also test 32 bit builds
NVER=`node -v`
wget http://nodejs.org/dist/${NVER}/node-${NVER}-${platform}-x86.tar.gz
tar xf node-${NVER}-${platform}-x86.tar.gz
# enable 32 bit node
export PATH=$(pwd)/node-${NVER}-${platform}-x86/bin:$PATH
# install 32 bit compiler toolchain and X11
# test source compile in 32 bit mode
CC=gcc-4.6 CXX=g++-4.6 npm install --build-from-source
node-pre-gyp package testpackage
npm test
npm run gh-publish
npm run clean

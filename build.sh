#!/usr/bin/env bash

if [ ! -d "./out" ]; then
    mkdir out
fi

pushd out

echo "Running CMake"
cmake -GNinja ..

echo "Running Make"
ninja

popd


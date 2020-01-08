#!/usr/bin/env bash

if [ ! -d "./out" ]; then
    mkdir out
fi

pushd out

echo "Running CMake"
cmake -GNinja ..

echo "Running Ninja"
if echo $* | grep -e "clean" -q
then
    ninja clean
elif echo $* | grep -e "rebuild" -q
then
    ninja clean
    ninja
else
    ninja
fi

popd


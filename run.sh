#!/usr/bin/env bash

if [ -d "out" ] && [ -d "out/Blaze" ]; then
    pushd ./out/Blaze/
    ./Blaze
    popd
else
    echo "Build blaze first."
fi


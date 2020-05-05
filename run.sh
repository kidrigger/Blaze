#!/usr/bin/env bash

if [ -d "out" ] && [ -d "out/Blaze" ]; then
    pushd ./out/Blaze/
    if echo $* | grep -e "debug" -q
    then
        lldb Blaze
    else
        ./Blaze
    fi
    popd
else
    echo "Build blaze first."
fi

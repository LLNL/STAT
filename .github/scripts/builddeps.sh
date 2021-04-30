#!/bin/bash

wget https://github.com/LLNL/graphlib/archive/refs/tags/v3.0.0.tar.gz
tar xf v3.0.0.tar.gz
pushd graphlib-3.0.0
    mkdir build
    pushd build
        cmake -D CMAKE_INSTALL_PREFIX=$HOME/local ..
        make -j
        make install
    popd
popd
ls -l $HOME/local/
ls -l $HOME/local/lib

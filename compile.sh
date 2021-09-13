#!/bin/bash

# make sure everything is up to date
git pull

# compile
scons --cfg=build/config-linux-gcc.py -j8
if [ $? -ne 0 ]
then
    echo Compilation failed.
    echo If the configuration failed, make sure that you installed all prerequesites.
    echo Have a look at install_prerequesites_ubuntu.sh
    echo
    echo Please report back any issues you encountered.
    exit 1
fi

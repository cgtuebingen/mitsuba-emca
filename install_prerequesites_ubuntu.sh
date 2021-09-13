#!/bin/bash

# update repository cache
sudo apt-get update

# core build environment
sudo apt-get install -y build-essential scons
# image libraries
sudo apt-get install -y libpng-dev libjpeg-dev libilmbase-dev libopenexr-dev
# math libraries
sudo apt-get install -y libeigen3-dev libfftw3-dev
# boost
sudo apt-get install -y libboost-all-dev
# xml library
sudo apt-get install -y libxerces-c-dev
# regexp library
sudo apt-get install -y libpcrecpp0v5

## GUI stuff - not really needed to reproduce the results
# graphics libraries
sudo apt-get install -y libxxf86vm-dev libglew-dev
# qt
sudo apt-get install -y qt5-default qt5-qmake libqt5xmlpatterns5-dev

## utilities
sudo apt-get install -y wget unzip

## python 3 and modules required for EMCA
sudo apt-get install -y python3 python3-pip
sudo -H python3 -m pip install --upgrade pip
sudo -H python3 -m pip install Imath numpy six scipy matplotlib OpenEXR Pillow PySide2 vtk

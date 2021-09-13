Mitsuba-EMCA
============

This repository contains example code for integrating the [EMCA](https://github.com/cgtuebingen/emca) Plugin into [Mitsuba](https://github.com/mitsuba-renderer/mitsuba).

## Compiling the EMCA Server Library
Before compiling Mitsuba-EMCA, first compile and install the [EMCA server library](https://github.com/cgtuebingen/emca/tree/public/server).
If you cannot install the library into the system-wide directories, you can instead install the library into a local prefix
and adjust the paths `EMCAINCLUDE` and `EMCALIBDIR` in the configuration files in the `build` folder.

## Compiling Mitsuba
First, make sure that the EMCA Server library is installed.
For compilation, have a look at the [Mitsuba documentation](http://mitsuba-renderer.org/docs.html).
If you're running Ubuntu, you can install the remaining required prerequisites using `install_prerequesites_ubuntu.sh`.

With the prerequisites installed, the code can be compiled on Linux using SCons: (append `-jX` for parallel compilation, e.g. `-j8`)

    scons --cfg=build/config-linux-gcc.py

We have merged and extended the patch for Python 3 compatibility, so that recent versions of SCons should work as well.
Let us know if any changes to the configuration files are necessary for other operating systems.

We have tested compilation on Ubuntu 20.04.

EMCA uses C++17 features (`std::variant`), which may make it a bit more tricky to compile.
You need a recent version of OpenEXR that compiles with C++17, e.g. version 2.3.0.
Older versions use [deprecated dynamic exception specifications](https://github.com/AcademySoftwareFoundation/openexr/issues/235) that will not compile.

### Compiling the GUI
For compiling the GUI, check the configuration for Qt5 in `data/scons/qt5.py`.
We have modified it for our systems, where all Qt packages start with a *Qt5* prefix rather than just *Qt*.
To simplify compilation on modern systems, we have also replaced GLEW-mx with GLEW.

## Running Mitsuba-EMCA
To run Mitsuba-EMCA, you need to use the command-line utility `./dist/mtsutil emca <scene.xml>`.
Before running the EMCA utility, you need to add the `dist` folder to the `LD_LIBRARY_PATH`.
Otherwise, loading Mitsuba plugins will fail.
The simplest way to do this is using the `setpath.sh` script:

    source setpath.sh
    ./dist/mtsutil emca <scene.xml>

You might additionally need to add the path to the EMCA Server library to the `LD_LIBRARY_PATH`, if not installed to the default location.

## Integration of EMCA into Mitsuba
All you need to do to add support for EMCA to your path tracer is to add some instrumentation code as explained in the following section.

### Adjusting the Integrator for EMCA
As an example, there is a copy of the default path tracer `src/integrators/path/path.cpp` at `src/integrators/path/pathemca.cpp` with added instrumentation for collecting the data needed by the EMCA client.
When using EMCA, make sure that your integrator is instrumented accordingly.
Otherwise, you will not have access to any path data when using the client.

### Deterministic Path Tracing
The EMCA utility automatically forces the use of a deterministic sampler, which is implemented in `src/samplers/deterministic.cpp` as a copy of the independent sampler.
At each pixel, this sampler is seeded based on the pixel location to produce repeatable outcomes.
The sampler defined in the scene's xml file is only used to determine the sample count.

### Implementation of the EMCA utility
The EMCA utility is implemented in `src/utils/emca.cpp` and interfaces with the EMCA server library and Mitsuba.
To support Mitsuba's native types, it provides a specailization of emca::DataApi defined in `include/mitsuba/core/dataapimitsuba.h` and implemented in `src/libcore/dataapimitsuba.cpp`.

## License
The code is licensed under GNU GPLv3, see LICENSE for details.

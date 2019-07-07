Build the ShapeGen demo to run on SDL2 in Linux
===============================================

_Author:_ Jerry R. VanAken
_Date:_ 7/6/2019

This directory (the linux-sdl subdirectory in your ShapeGen installation) contains the files you'll need to build the version of the ShapeGen demo program that runs on [SDL2](https://wiki.libsdl.org/FrontPage) (version 2 of the Simple DirectMedia Layer) in Linux.

The ShapeGen demo runs in a 1280-by-960 window. The demo displays a new image each time you press the space bar. Each image -- including both text and graphics -- is constructed entirely by an instance of the ShapeGen class.

## What's in this directory

This directory (i.e., the linux-sdl subdirectory in your ShapeGen demo installation) contains these four files:

* `README.md` -- This README file

* `Makefile` -- A makefile that is invoked with the GNU make utility, and that contains GNU g++ compiler and linker commands

* `sdlmain.cpp` -- Contains the platform-specific code necessary to run the ShapeGen demo on SDL2 in Linux

## Build the ShapeGen demo

Follow these steps to build and run the ShapeGen demo from the command line:

1. Open a terminal window.
2. Install the GNU g++ compiler/linker and GNU make utilities, if you haven't done so already.
3. Install SDL2, if you haven't done so already (more information below).
4. Change to _this_ directory (i.e., the linux-sdl subdirectory in your ShapeGen installation).
5. Enter the command "make" to build the demo.
6. Enter the command "./demo" to run the demo.

## Installing SDL2

The [official SDL2 website](https://wiki.libsdl.org) provides instructions for installing SDL2 on various platforms. The [Installing SDL](https://wiki.libsdl.org/Installation) page at this website explains that

>Debian-based systems (including Ubuntu) can simply do "sudo apt-get install libsdl2-2.0" to get the library installed system-wide, and all sorts of other useful dependencies, too.


.

.

.

.
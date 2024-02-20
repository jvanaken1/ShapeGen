# Build the ShapeGen demo to run on SDL2 in Linux

_Author:_ Jerry R. VanAken  
_Date:_ 2/19/2024

This directory (the `linux-sdl` subdirectory in your ShapeGen installation) contains the files you'll need to build the versions of the ShapeGen demo program (`demo`) and SVG file viewer (`svgview`) that run on [SDL2](https://wiki.libsdl.org/FrontPage) (version 2 of the Simple DirectMedia Layer) in Linux.

The ShapeGen demo runs in a 1280-by-960 window. The demo displays a new image each time you press the space bar. Each image -- including both text and graphics -- is constructed entirely by the ShapeGen 2-D vector graphics library.

One convenient way to copy the ShapeGen project files to your system is to install the Git version control system, and then enter the command  
    `git clone https://github.com/jvanaken1/shapegen`  
in your terminal window. This command creates the `shapegen` subdirectory in the current directory and copies the ShapeGen project files to the new subdirectory.


## What's in this directory

This directory (the `linux-sdl` subdirectory in your ShapeGen installation) initially contains just these three files:

* `README.md` -- This README file

* `Makefile` -- A makefile that is invoked with the GNU make utility, and that contains GNU g++ compiler and linker commands

* `sdlmain.cpp` -- Contains the platform-specific code necessary to run the ShapeGen demo on SDL2 in Linux

## Build the ShapeGen demo

Follow these steps to build and run the ShapeGen demo from the command line:

1. Open a terminal window.
2. Install the GNU g++ compiler/linker and GNU make utilities, if you haven't done so already.
3. Install SDL2, if you haven't done so already (more information below).
4. Change to _this_ directory (the `linux-sdl` subdirectory in your ShapeGen project files).
5. Enter the command `make` to build the demo.
6. Enter the command `./demo` to run the demo.

The `make` command also builds `svgview`, the SVG file viewer. To test `svgview`, you'll need to provide it with a list of one or more SVG files. For more information, see Appendix B in the _ShapeGen User's Guide_ (the `userdoc.pdf` file in this project's main directory).

## Installing SDL2

The [official SDL2 website](https://wiki.libsdl.org) provides instructions for installing SDL2 on various platforms. The [Installing SDL](https://wiki.libsdl.org/SDL2/Installation#linuxunix) page at this website explains how to install developer versions of SDL2 on various Linux distributions, including Debian-based systems (such as Ubuntu), Red Hat-based systems (such as Fedora), and Gentoo. Note that you'll need to install the _developer_ version of SDL2 in order to build the ShapeGen `demo` and `svgview` example apps.

.

.

.

.

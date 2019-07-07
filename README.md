ShapeGen: C++ implementation of a 2-D polygonal shape generator
===============================================================

_Author:_ Jerry R. VanAken  
_Date:_ 7/4/2019

The polygonal shape generator is the part of a 2-D graphics system that takes graphical shapes specified in terms of curves, arcs, and line segments, and converts these shapes to lists of polygonal edges that can be passed to a renderer and filled on a graphics display.

This GitHub project contains the C++ source code for the ShapeGen class, which is a portable, lightweight polygonal shape generator that has these features:

* The boundaries of arbitrary shapes can be specified with any combination of line segments, rectangles, ellipses, elliptic arcs, and quadratic and cubic Bezier curves.

* Shapes can be filled or stroked. 

* Shapes can be filled according to either the even-odd (aka parity) fill rule, or the nonzero-winding-number fill rule.

* Stroked shapes can be constructed with user-specified line widths, dashed-line patterns, joint styles, and line ends.

* The _current clipping region_ automatically clips both filled and stroked shapes before they are rendered.

* Clipping regions can be set to arbitrarily shaped areas. And arbitrarily shaped areas can be masked off from the clipping region. 

* By default, users specify shapes with integer x-y coordinates, but they can optionally be specified with fixed-point coordinate values if additional precision is required.

* The ShapeGen class implementation is neither platform-specific nor device-dependent, and is readily ported to any computing environment for which a C++ compiler is available.

## Demo program

Included with the ShapeGen source code is a demo program that shows off ShapeGen features. The demo code is configured to build and run in any of the following environments:

 * [SDL2](https://wiki.libsdl.org/) (Simple DirectMedia Layer) in Linux

 * SDL2 in Windows

 * Win32 API in Windows

The makefiles and platform-specific code for these environments are included in the linux-sdl, windows-sdl, and windows-gdi subdirectories in this project. Each subdirectory contains a README file with instructions for setting up the build environment and making the demo.

The window size for the demo is set at 1280-by-960.

## ShapeGen User's Guide

The userdoc.pdf file in this GitHub project contains the ShapeGen User's Guide. This guide contains a discussion of general principles of operation, detailed descriptions of the functions in the ShapeGen programming interface, and numerous code examples. The source code for these code examples is also included in the demo.cpp file, and is executed at the end of the demo program.

## Project files

The source files in the main directory of this GitHub project contain no platform-specific code. The small amount of platform-specific code needed for the demo program is relegated to the linux-sdl, windows-sdl, and windows-gdi subdirectories.

This GitHub project includes the following files and directories.

**Main directory**

* `README.md` -- This README file

* `userdoc.pdf` -- ShapeGen User's Guide

* `arc.cpp` -- ShapeGen member functions for adding ellipses, elliptic arcs, elliptic splines, and rounded rectangles to paths

* `curve.cpp` -- ShapeGen member functions for adding quadratic and cubic Bezier spline curves to paths
 
* `demo.cpp` -- ShapeGen application code called by the demo program
 
* `edge.cpp` -- ShapeGen member functions for converting paths to lists of polygonal edges, for clipping shapes defined by polygonal edge lists, and for feeding shape information to renderers
 
* `path.cpp` -- ShapeGen member functions for managing path construction, for setting path attributes, and for adding line segments and rectangles to paths 

* `stroke.cpp` -- ShapeGen member functions for stroking paths, and for setting the attributes of stroked paths
 
* `textapp.cpp` -- Application code that uses the ShapeGen functions to construct the simple graphical text used in the demo program
 
* `thinline.cpp` -- ShapeGen internal code for constructing thin, stroked line segments that mimic lines drawn by the Bresenham line algorithm
 
* `demo.h` -- Header file for the demo program
 
* `shapegen.h` -- ShapeGen public header file
 
* `shapepri.h` -- ShapeGen private header file

**linux-sdl subdirectory**

* `README.md` -- Instructions for installing the build environment and making the version of the ShapeGen demo that runs on SDL2 in Linux

* `Makefile` -- The makefile that builds the ShapeGen demo for SDL2 in Linux

* `sdlmain.cpp` -- Contains the SDL2 main program, a simple renderer, and all the platform-specific code needed to run the ShapeGen demo on SDL2 in Linux

**windows-sdl subdirectory**

* `README.md` -- Instructions for installing the build environment and making the version of the ShapeGen demo that runs on SDL2 in Windows

* `Makefile` -- The makefile that builds the ShapeGen demo for SDL2 in Windows

* `sdlmain.cpp` -- Contains the SDL2 main program, a simple renderer, and all the platform-specific code needed to run the ShapeGen demo on SDL2 in Windows

* `sdlpath.cmd` -- Adds the directory containing the SDL2 DLL files to the path environment variable in Windows

**windows-gdi subdirectory**

* `README.md` -- Instructions for installing the build environment and making the version of the ShapeGen demo that runs on the Win32 API in Windows

* `Makefile` -- The makefile that builds the ShapeGen demo for the Win32 API in Windows

* `winmain.cpp` -- Contains the SDL2 main program, a simple renderer, and all the platform-specific code needed to run the ShapeGen demo on the Win32 API in Windows

.

.

.

.

.





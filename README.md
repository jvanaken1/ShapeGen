ShapeGen 2-D Graphics Library
-------------

_Author:_ Jerry R. VanAken  
_Date:_ 9/15/2023

**Breaking Change &ndash; September 15, 2023:** Several minor (but breaking) changes have been made to the ShapeGen API. For more information, see the _ShapeGen User's Guide_ in the `userdoc.pdf` file in the main directory of this project.

## Overview

This GitHub project contains the C++ source code for the ShapeGen 2-D graphics library.

Library users call ShapeGen functions to construct paths that define arbitrarily complex shapes. Shape boundaries are formed by connecting line segments, spline curves, and circular and elliptic arcs. Shapes can be filled or stroked with solid colors, tiled patterns, linear gradients, and radial gradients. For stroked paths, users can specify line widths, join and cap styles, and line-dash patterns. Clipping regions can have arbitrary shape and complexity. The example renderers support true-color displays and provide antialiasing and alpha blending.

The ShapeGen library is lightweight, and is readily ported to any computing environment for which a C++ compiler and Standard C Library are available.

All of the C++ source files in the main directory of this project are free of platform or device dependences. The only platform-dependent code in the project is relegated to the single C++ source file (sdlmain.cpp or winmain.cpp) in the linux-sdl, windows-sdl, or windows-gdi subdirectory. This platform-dependent code is needed to run the example ShapeGen-based applications on the [SDL2 API](https://wiki.libsdl.org/) in Linux and Windows, and on the Win32/GDI API in Windows.

The two example applications in this project are:
* A demo program (`demo` or `demo.exe`) to show off the graphics capabilities of the ShapeGen library
* A simple ShapeGen-based SVG file viewer (`svgview` or `svgview.exe`) for viewing standalone SVG files (with `.svg` filename extensions)

These applications require a true-color graphics display, and run in a 1280x960 window. Make files and step-by-step instructions are provided for building all three platform-specific versions of these applications. 

For more information, see the _ShapeGen User's Guide_ (the userdoc.pdf file in the project's main directory). This document explains key concepts, provides a comprehensive API reference, and contains many code examples. These code examples are also included at the end of the ShapeGen demo program.

## Project files

This GitHub project includes the following files and directories.

**Main directory**

* `README.md` &ndash; This README file

* `userdoc.pdf` &ndash; _ShapeGen User's Guide_

* `alfablur.cpp` &ndash; Example code to show how to filter images

* `arc.cpp` &ndash; ShapeGen public and private member functions for adding ellipses, elliptic arcs, elliptic splines, and rounded rectangles to paths

* `bmpfile.cpp` &ndash; Rudimentary BMP file reader used for tiled-pattern fills in ShapeGen demo program

* `curve.cpp` &ndash; ShapeGen public and private member functions for adding quadratic and cubic Bezier spline curves to paths
 
* `demo.cpp` &ndash; Example ShapeGen application code for the demo program
 
* `edge.cpp` &ndash; ShapeGen internal code for converting paths to lists of polygonal edges, for clipping shapes defined by polygonal edge lists, and for feeding shape information to renderers

* `gradient.cpp` &ndash; Paint generators for filling and stroking shapes with linear gradients and radial gradients

* `nanosvg.h` &ndash; A modified version of M. Mononen's single-header-file SVG file parser
 
* `path.cpp` &ndash; ShapeGen public and private member functions for managing path construction, for setting path attributes, and for adding line segments and rectangles to paths

* `pattern.cpp` &ndash; Paint generator for filling and stroking shapes with tiled patterns

* `renderer.cpp` &ndash; Platform-independent renderer implementation for true-color graphics displays

* `stroke.cpp` &ndash; ShapeGen public and private member functions for stroking paths, and for setting the attributes of stroked paths

* `svgview.cpp` &ndash; Example ShapeGen application code for the SVG file viewer
 
* `textapp.cpp` &ndash; Example application code that uses the ShapeGen functions to construct the simple graphical text used in the demo program
 
* `thinline.cpp` &ndash; ShapeGen internal code for constructing thin, stroked line segments that mimic the appearance of lines drawn by the [Bresenham line algorithm](https://en.wikipedia.org/wiki/Bresenham's_line_algorithm)
 
* `demo.h` &ndash; Header file for this project's example ShapeGen-based applications

* `renderer.h` &ndash; Header file defining the renderer's interfaces to the shape generator, paint generators, and applications
 
* `shapegen.h` &ndash; ShapeGen header file for public interfaces
 
* `shapepri.h` &ndash; ShapeGen header file for internal interfaces

**linux-sdl subdirectory**

* `README.md` &ndash; Instructions for building the example ShapeGen applications to run on SDL2 in Linux

* `Makefile` &ndash; The make file that builds the example ShapeGen applications to run on SDL2 in Linux

* `sdlmain.cpp` &ndash; Contains the SDL2 main program and all the platform-specific code needed to run the example ShapeGen applications on SDL2 in Linux

**windows-sdl subdirectory**

* `README.md` &ndash; Instructions for building the example ShapeGen applications to run on SDL2 in Windows

* `Makefile` &ndash; The make file that builds the example ShapeGen applications to run on SDL2 in Windows

* `sdlmain.cpp` &ndash; Contains the SDL2 main program and all the platform-specific code needed to run the example ShapeGen applications on SDL2 in Windows

* `sdlpath.cmd` &ndash; Adds the directory containing the SDL2 DLL files to the path environment variable in Windows

**windows-gdi subdirectory**

* `README.md` &ndash; Instructions for building the example ShapeGen applications to run on Win32/GDI in Windows

* `Makefile` &ndash; The make file that builds the example ShapeGen applications to run on Win32/GDI in Windows

* `winmain.cpp` &ndash; Contains the WinMain program and all the platform-specific code needed to run the example ShapeGen applications on Win32/GDI in Windows

.

.

.

.

.

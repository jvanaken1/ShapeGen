# ShapeGen Open-Source 2-D Vector Graphics Library

_Author:_ Jerry R. VanAken  
_Date:_ 3/30/2024

![Conic gradient example](/data/logo.jpg?raw=true)

## Overview

This GitHub project contains the C++ source code for the ShapeGen 2-D vector graphics library. The library is lightweight and highly portable.

ShapeGen draws both filled and stroked shapes. Shape boundaries (paths) of arbitrary complexity are constructed by connecting lines, Bezier curves (quadratic and cubic), and circular and elliptical arcs. Boundaries can contain holes, have disjoint regions, and self-intersect. Clipping regions can be defined with the same flexibility as shape boundaries.

The appearance of a stroked shape (path) is controlled by the following attributes:
* line width
* dashed-line pattern
* line-join style
* line-end cap style
* miter limit

The library’s rendering code provides antialiasing and alpha blending, and can fill shapes with solid colors, tiled patterns (2-D textures), linear gradients, radial gradients, and conic gradients. Arbitrary affine transformations can be automatically applied to patterns and gradients.

Some 2-D graphics libraries achieve portability by sitting atop a portable graphics library such as OpenGL. Or perhaps they run on Cairo, which in turn runs on PixMan. The resulting dependencies complicate the build process and create bloated executables.

The ShapeGen library code has only two dependencies. The library requires
* a C++ compiler
* the Standard C Library

To achieve portability, ShapeGen draws directly to pixel memory, with absolutely no reliance on intermediaries to do the actual rendering.

Of course, an operating system such as Linux or Windows does not allow an application program to draw directly to the on-screen memory for a windowed display that is shared with other applications. To run in these systems, a small amount of platform-dependent code is required to copy an image to on-screen memory after ShapeGen has finished rendering the image.

All such platform-dependent code in this project is isolated in the source files `winmain.cpp` and `sdlmain.cpp`. These files enable the example ShapeGen-based applications in this project to run on the [SDL2 API](https://wiki.libsdl.org/) in Linux and Windows, and on the Win32/GDI API in Windows.

This project's two example applications are
* a demo program (`demo` or `demo.exe`) to show off the graphics capabilities of the ShapeGen library
* a simple ShapeGen-based SVG file viewer (`svgview` or `svgview.exe`) for viewing standalone SVG files (with `.svg` filename extensions)

These applications require a true-color graphics display, and run in a 1280x960 window. Make files and step-by-step instructions are provided for building all three platform-specific versions of these applications.

Minimal effort is required to port the ShapeGen library to a system whose graphics display device has limited color capabilities and does not support true-color pixel formats. For this type of system, a simple renderer is sufficient. This renderer consists of only a few lines of code and does only solid-color fills, with no antialiasing or alpha blending. 

A renderer is not required to do clipping. All shapes are clipped before being handed off to the renderer.

For more information, see the _ShapeGen User's Guide_ (the `userdoc.pdf` file in the project's main directory). This document explains key concepts, provides a comprehensive API reference, and contains many code examples. These code examples are also included at the end of the ShapeGen demo program.

## Project files

This GitHub project includes the following files and directories.

**Main directory**

* `README.md` &ndash; This README file

* `userdoc.pdf` &ndash; _ShapeGen User's Guide_

* `alfablur.cpp` &ndash; Example code to show how to filter images

* `arc.cpp` &ndash; ShapeGen public and private member functions for adding ellipses, elliptical arcs, elliptical splines, and rounded rectangles to paths

* `bmpfile.cpp` &ndash; Rudimentary BMP file reader used for tiled-pattern fills in ShapeGen demo program

* `curve.cpp` &ndash; ShapeGen public and private member functions for adding quadratic and cubic Bezier spline curves to paths
 
* `demo.cpp` &ndash; Example ShapeGen application code for the demo program
 
* `edge.cpp` &ndash; ShapeGen internal code for converting paths to lists of polygonal edges, for clipping shapes defined by polygonal edge lists, and for feeding shape information to renderers

* `gradient.cpp` &ndash; Paint generators for filling and stroking shapes with linear gradients, radial gradients, and conic gradients

* `nanosvg.h` &ndash; Modified version of M. Mononen's single-header-file SVG file parser
 
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

* `Makefile` &ndash; Make file that builds the example ShapeGen applications to run on SDL2 in Linux

* `sdlmain.cpp` &ndash; Contains the SDL2 main program and all the platform-specific code needed to run the example ShapeGen applications on SDL2 in Linux

**windows-sdl subdirectory**

* `README.md` &ndash; Instructions for building the example ShapeGen applications to run on SDL2 in Windows

* `Makefile` &ndash; Make file that builds the example ShapeGen applications to run on SDL2 in Windows

* `sdlmain.cpp` &ndash; Contains the SDL2 main program and all the platform-specific code needed to run the example ShapeGen applications on SDL2 in Windows

* `sdlpath.cmd` &ndash; Adds the directory containing the SDL2 DLL files to the path environment variable in Windows

**windows-gdi subdirectory**

* `README.md` &ndash; Instructions for building the example ShapeGen applications to run on Win32/GDI in Windows

* `Makefile` &ndash; Make file that builds the example ShapeGen applications to run on Win32/GDI in Windows

* `winmain.cpp` &ndash; Contains the WinMain program and all the platform-specific code needed to run the example ShapeGen applications on Win32/GDI in Windows

.

.

.

.

.

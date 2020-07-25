ShapeGen: C++ implementation of a 2-D polygonal shape generator
===============================================================

_Author:_ Jerry R. VanAken  
_Date:_ 7/25/2020

The polygonal shape generator is the part of a 2-D graphics system that takes a graphical shape specified in terms of curves, arcs, and line segments, and converts this shape to a list of nonoverlapping trapezoids that can be passed to a renderer and filled on a graphics display.

The geometric primitives in the ShapeGen programming interface are similar to the path-construction operators in the [PostScript language](https://www.adobe.com/content/dam/acom/en/devnet/actionscript/articles/psrefman.pdf).

This GitHub project contains the C++ source code for the ShapeGen class, which is a portable, lightweight polygonal shape generator that has these features:

* Shape boundaries can be specified with any combination of line segments, rectangles, ellipses, elliptic arcs, and quadratic and cubic Bezier curves.

* Shapes can be filled or stroked. 

* Shapes can be filled according to either the even-odd (aka parity) fill rule, or the nonzero-winding-number fill rule.

* Stroked shapes can be constructed with user-specified line widths, dashed line patterns, joint styles, and line caps.

* Both filled and stroked shapes are automatically clipped before they are rendered.

* Clipping regions can be set to arbitrarily shaped areas. And arbitrarily shaped areas can be masked off from the clipping region. 

* By default, users specify shapes with integer x-y coordinates. But shapes can optionally be specified with fixed-point coordinate values if additional precision is required.

* The ShapeGen class implementation is neither platform-specific nor device-dependent, and is readily ported to any computing environment for which a C++ compiler is available.

* Examples of both basic renderers (with no antialiasing) and antialiasing renderers are included in this GitHub project.

* Developers can easily replace the example renderers in this project with custom renderers that conform to a simple, well-defined interface.

## Demo program

Included with the source code in this GitHub project is a demo program that shows off ShapeGen features. The demo code is configured to build and run in any of the following environments:

 * [SDL2](https://wiki.libsdl.org/) (Simple DirectMedia Layer) in Linux

 * SDL2 in Windows

 * Win32 API in Windows

The make files and platform-specific code for these environments are included in the linux-sdl, windows-sdl, and windows-gdi subdirectories in this project. Each subdirectory contains a README file with instructions for setting up the build environment and making the demo.

The window size for the demo is set at 1280-by-960.

## ShapeGen documentation

The `userdoc.pdf` file in this GitHub project contains the _ShapeGen User's Guide_. This guide discusses general principles of operation, describes the functions in the ShapeGen programming interface, and includes numerous code examples. The source code for these code examples is also included in the `demo.cpp` file, and is executed at the end of the demo program.

For a high-level overview of ShapeGen capabilities and internal operation, see [_ShapeGen: A lightweight, open-source 2-D graphics library written in C++_](https://www.researchgate.net/publication/341194243_ShapeGen_A_lightweight_open-source_2-D_graphics_library_written_in_C) on the ResearchGate website.

For an in-depth discussion of the parametric ellipse algorithm that ShapeGen uses to draw circles, ellipses, elliptic arcs, elliptic splines, and rounded rectangles, see [_A Fast Parametric Ellipse Algorithm_](https://www.researchgate.net/publication/343180244_A_Fast_Parametric_Ellipse_Algorithm) on the ResearchGate website.

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
 
* `thinline.cpp` -- ShapeGen internal code for constructing thin, stroked line segments that mimic the appearance of lines drawn by the [Bresenham line algorithm](https://en.wikipedia.org/wiki/Bresenham's_line_algorithm)
 
* `demo.h` -- Header file for the demo program
 
* `shapegen.h` -- ShapeGen public header file
 
* `shapepri.h` -- ShapeGen private header file

**linux-sdl subdirectory**

* `README.md` -- Instructions for installing the build environment and making the version of the ShapeGen demo that runs on SDL2 in Linux

* `Makefile` -- The make file that builds the ShapeGen demo for SDL2 in Linux

* `sdlmain.cpp` -- Contains the SDL2 main program, two example renderers, and all the platform-specific code needed to run the ShapeGen demo on SDL2 in Linux

**windows-sdl subdirectory**

* `README.md` -- Instructions for installing the build environment and making the version of the ShapeGen demo that runs on SDL2 in Windows

* `Makefile` -- The make file that builds the ShapeGen demo for SDL2 in Windows

* `sdlmain.cpp` -- Contains the SDL2 main program, two example renderers, and all the platform-specific code needed to run the ShapeGen demo on SDL2 in Windows

* `sdlpath.cmd` -- Adds the directory containing the SDL2 DLL files to the path environment variable in Windows

**windows-gdi subdirectory**

* `README.md` -- Instructions for installing the build environment and making the version of the ShapeGen demo that runs on the Win32 API in Windows

* `Makefile` -- The make file that builds the ShapeGen demo for the Win32 API in Windows

* `winmain.cpp` -- Contains the SDL2 main program, two example renderers, and all the platform-specific code needed to run the ShapeGen demo on the Win32 API in Windows

.

.

.

.

.




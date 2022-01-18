ShapeGen 2-D Graphics Library
-------------

_Author:_ Jerry R. VanAken  
_Date:_ 1/17/2022

This GitHub project contains the C++ source code for the ShapeGen 2-D graphics library. Library users construct shapes consisting of curves, arcs, and line segments. Shapes can be filled or stroked with solid colors, gradients, and patterns. The library supports antialiasing and alpha-blending. Clipping regions can have arbitrary shape and complexity.

The ShapeGen library is lightweight and highly portable.

All of the C++ source files in the main directory of this project are neither platform-specific nor device-dependent. Thus, the ShapeGen library is readily ported to any computing environment for which a C++ compiler is available.

The only platform-dependent code in the library is relegated to the single C++ source file (sdlmain.cpp or winmain.cpp) in the linux-sdl, windows-sdl, or windows-gdi subdirectory. The files in these subdirectories contain example renderers that run on [SDL2](https://wiki.libsdl.org/) in Linux and Windows, and on Win32 GDI in Windows. Two example renderers are provided for each of these platforms. The first is a _basic renderer_ that consists of just a few lines of code. The second is an _enhanced renderer_ that is intended for use with full-color displays. 

A basic renderer does only solid-color fills, and can run on a computer with limited graphics capabilities.

An enhanced renderer provides antialiasing, alpha-blending, pattern fills, linear gradient fills, and radial gradient fills. This renderer relies on the _paint generators_ in the main directory to do nearly all of the work required for pattern and gradient fill operations.

To further simplify renderer design, the ShapeGen library clips all shapes before passing them to the renderer.

The core of the ShapeGen library is the _polygonal shape generator_, which is implemented in the ShapeGen class. ShapeGen functions enable to the user to construct paths that specify graphical shapes consisting of curves, arcs, and line segments. The shape generator then converts each path to a list of nonoverlapping horizontal spans that can be passed to a renderer and filled on a graphics display.

Because the shape generator stops short of actually touching the pixels on the graphics display, the shape generator must be paired with a renderer to form a fully functioning graphics library. Whereas the renderer is necessarily device-dependent and/or platform-specific, the shape generator is free of such dependencies.

The geometric primitives in the ShapeGen graphics library are similar to the path-construction operators in the [PostScript language](https://www.adobe.com/content/dam/acom/en/devnet/actionscript/articles/psrefman.pdf). The ShapeGen graphics library has these features:

* Shape boundaries can be specified with any combination of line segments, rectangles, ellipses, elliptic arcs, and Bezier curves (both quadratic and cubic).

* Shapes can be filled or stroked.

* Shape boundaries can intersect. Shapes can include holes and disjoint regions.
 
* Both nonzero-winding-number and even-odd fill rules are supported.

* Stroked shapes can be constructed with user-specified line widths, dashed line patterns, join styles, and line caps.

* Both filled and stroked shapes are automatically clipped before they are rendered.

* Clipping regions can be set to arbitrarily shaped areas. In addition, arbitrarily shaped areas can be masked off from the clipping region. 

By default, ShapeGen library users specify shapes with integer _x-y_ coordinates. But shapes can optionally be specified with fixed-point coordinate values if additional precision is required. Thus, developers can test their custom algorithms for drawing curves and arcs free of coordinate round-off or truncation errors.

## Demo program

Included with the source code in this GitHub project is a demo program that shows off ShapeGen features. The demo code is configured to build and run in any of the following environments:

 * [SDL2](https://wiki.libsdl.org/) (Simple DirectMedia Layer) in Linux

 * SDL2 in Windows

 * Win32 API in Windows

The make files and platform-specific code for these environments are included in the linux-sdl, windows-sdl, and windows-gdi subdirectories in this project. Each subdirectory contains a README file with instructions for setting up the build environment and making the demo.

The window size for the demo is set at 1280-by-960.

## ShapeGen documentation

The userdoc.pdf file in this GitHub project contains the _ShapeGen User's Guide_. This guide discusses general principles of operation, contains detailed reference pages for the functions in the ShapeGen programming interface, and includes numerous code examples. The source code for these code examples is also included in the `demo.cpp` file, and is executed at the end of the demo program.

For a high-level overview of ShapeGen capabilities and internal operation, see [_ShapeGen: A lightweight, open-source 2&minus;D graphics library written in C++_](https://www.researchgate.net/publication/341194243_ShapeGen_A_lightweight_open-source_2D_graphics_library_written_in_C) on the ResearchGate website.

For an in-depth discussion of the parametric ellipse algorithm that ShapeGen uses to draw circles, ellipses, elliptic arcs, elliptic splines, and rounded rectangles, see [_A Fast Parametric Ellipse Algorithm_](https://arxiv.org/abs/2009.03434) on the arXiv website.

## Project files

A previously mentioned, the C++ source files in the main directory of this GitHub project contain no platform-specific code. The small amount of platform-specific code needed for the demo program is relegated to the linux-sdl, windows-sdl, and windows-gdi subdirectories.

This GitHub project includes the following files and directories.

**Main directory**

* `README.md` &ndash; This README file

* `userdoc.pdf` &ndash; _ShapeGen User's Guide_

* `arc.cpp` &ndash; ShapeGen member functions for adding ellipses, elliptic arcs, elliptic splines, and rounded rectangles to paths

* `bmpfile.cpp` &ndash; Rudimentary BMP file reader used in demo code for tiled-pattern fills

* `curve.cpp` &ndash; ShapeGen member functions for adding quadratic and cubic Bezier spline curves to paths
 
* `demo.cpp` &ndash; ShapeGen application code called by the demo program
 
* `edge.cpp` &ndash; ShapeGen member functions for converting paths to lists of polygonal edges, for clipping shapes defined by polygonal edge lists, and for feeding shape information to renderers

* `gradient.cpp` &ndash; Paint generators for filling and stroking shapes with linear gradients and radial gradients
 
* `path.cpp` &ndash; ShapeGen member functions for managing path construction, for setting path attributes, and for adding line segments and rectangles to paths

* `pattern.cpp` &ndash; Paint generator for filling and stroking shapes with tiled patterns

* `stroke.cpp` &ndash; ShapeGen member functions for stroking paths, and for setting the attributes of stroked paths
 
* `textapp.cpp` &ndash; Application code that uses the ShapeGen functions to construct the simple graphical text used in the demo program
 
* `thinline.cpp` &ndash; ShapeGen internal code for constructing thin, stroked line segments that mimic the appearance of lines drawn by the [Bresenham line algorithm](https://en.wikipedia.org/wiki/Bresenham's_line_algorithm)
 
* `demo.h` &ndash; Header file for the demo program

* `renderer.h` &ndash; Header file defining the renderer's interfaces to the shape generator and paint generators
 
* `shapegen.h` &ndash; ShapeGen public header file
 
* `shapepri.h` &ndash; ShapeGen private header file

**linux-sdl subdirectory**

* `README.md` &ndash; Instructions for installing the build environment and making the version of the ShapeGen demo that runs on SDL2 in Linux

* `Makefile` &ndash; The make file that builds the ShapeGen demo for SDL2 in Linux

* `sdlmain.cpp` &ndash; Contains the SDL2 main program, two example renderers, and all the platform-specific code needed to run the ShapeGen demo on SDL2 in Linux

**windows-sdl subdirectory**

* `README.md` &ndash; Instructions for installing the build environment and making the version of the ShapeGen demo that runs on SDL2 in Windows

* `Makefile` &ndash; The make file that builds the ShapeGen demo for SDL2 in Windows

* `sdlmain.cpp` &ndash; Contains the SDL2 main program, two example renderers, and all the platform-specific code needed to run the ShapeGen demo on SDL2 in Windows

* `sdlpath.cmd` &ndash; Adds the directory containing the SDL2 DLL files to the path environment variable in Windows

**windows-gdi subdirectory**

* `README.md` &ndash; Instructions for installing the build environment and making the version of the ShapeGen demo that runs on the Win32 API in Windows

* `Makefile` &ndash; The make file that builds the ShapeGen demo for the Win32 API in Windows

* `winmain.cpp` &ndash; Contains the WinMain program, two example renderers, and all the platform-specific code needed to run the ShapeGen demo on the Win32 API in Windows

.

.

.

.

.



# Build the ShapeGen demo to run on SDL2 in Windows

_Author:_ Jerry R. VanAken  
_Date:_ 2/22/2024

This directory (the `windows-sdl` subdirectory in your ShapeGen installation) contains the files you'll need to build the version of the ShapeGen demo program that runs on [SDL2](https://wiki.libsdl.org/FrontPage) (version 2 of the Simple DirectMedia Layer) in Windows.

The ShapeGen demo runs in a 1280-by-960 window. The demo displays a new image each time you press the space bar. Each image -- including both text and graphics -- is constructed entirely by the ShapeGen 2-D vector graphics library.

## What's in this directory

The `windows-sdl` subdirectory in your ShapeGen installation initially contains just these four files:

* `README.md` -- This README file

* `Makefile` -- A makefile that is invoked with the Microsoft `nmake.exe` utility, and that contains Microsoft Visual Studio C/C++ compiler and linker commands

* `sdlmain.cpp` -- Contains the platform-specific code necessary to run the ShapeGen demo on SDL2 in Windows

* `sdlpath.bat` -- A batch file that adds the location of the SDL2 DLL files to the `path` environment variable

## Build the ShapeGen demo

Follow these steps to build and run the ShapeGen demo from the command line:

1. Install the free version of Microsoft Visual Studio (see instructions below), if you haven't already done so.
2. Download the latest developer version of SDL2 (see instructions below), if you haven't already done so. 
>Note: If you choose a directory other than "C:\SDL2" for your SDL2 installation, you'll need to update the `Makefile` and `sdlpath.bat` files in _this_ directory (that is, the `windows-sdl` directory in your ShapeGen installation files).
3. To open the Command Window that was installed with Visual Studio, first open the Windows Start menu. Then scroll down to and click on "Visual Studio 2022", which will display a drop-down list. From this list, select "Developer Command Prompt for VS 2022" (assuming that you will use the same PC to both build and run the demo).
4. In the Command Window, change to _this_ directory (that is, `windows-sdl`).
5. Enter the command `nmake` to build the `demo.exe` executable file.
6. Run the `sdlpath.bat` file in _this_ directory. This file sets the path for the SDL2 DLL files.
7. Enter the command `demo` to run the demo.

The `nmake` command also builds the SVG file viewer, `svgview.exe`. To test `svgview.exe`, you'll need to provide it with a list of one or more SVG files. For more information, see Appendix B in the _ShapeGen User's Guide_ (the `userdoc.pdf` file in this project's main directory).

## Downloading Visual Studio

The makefile in this directory was tested on Windows 10 with the following *free* version of Visual Studio:
* Microsoft Visual Studio Community 2022, Version 17.7.1

The download site for Visual Studio is currently [https://visualstudio.microsoft.com/vs/](https://visualstudio.microsoft.com/vs/)

## Downloading SDL2

Download the current stable version of SDL2 from [the official SDL2 download](https://github.com/libsdl-org/SDL/releases/tag/release-2.30.0) page. Select the SDL2 developer package for Visual Studio C/C++ (at time of writing, [SDL2-devel-2.30.0-VC.zip](https://github.com/libsdl-org/SDL/releases/download/release-2.30.0/SDL2-devel-2.30.0-VC.zip)). This download contains the SDL2 headers, plus the import libraries and DLLs for x86 and x64 builds. 

Create a directory (for example, `C:\SDL2`) to copy the SDL2 files to.

>Note: The `Makefile` and `sdlpath.bat` files in _this_ directory (the `windows-sdl` directory in your ShapeGen installation files) assume that the SDL2 files are located in a directory named `C:\SDL2`. If you choose a different directory for your SDL2 installation, be sure to update the `Makefile` and `sdlpath.bat` files.

For more information, see the [Installing SDL](https://wiki.libsdl.org/Installation) page at the official SDL2 website.

.

.

.

.

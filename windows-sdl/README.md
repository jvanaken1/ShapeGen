Build the ShapeGen demo to run on SDL2 in Windows
=================================================

_Author:_ Jerry R. VanAken  
_Date:_ 7/6/2019

This directory (the windows-sdl subdirectory in your ShapeGen installation) contains the files you'll need to build the version of the ShapeGen demo program that runs on [SDL2](https://wiki.libsdl.org/FrontPage) (version 2 of the Simple DirectMedia Layer) in Windows.

The ShapeGen demo runs in a 1280-by-960 window. The demo displays a new image each time you press the space bar. Each image -- including both text and graphics -- is constructed entirely by an instance of the ShapeGen class.

## What's in this directory

The windows-sdl subdirectory in your ShapeGen installation contains these four files:

* `README.md` -- This README file

* `Makefile` -- A makefile that is invoked with the Microsoft nmake utility, and that contains Microsoft Visual Studio C/C++ compiler and linker commands

* `sdlmain.cpp` -- Contains the platform-specific code necessary to run the ShapeGen demo on SDL2 in Windows

* `sdlpath.bat` -- A batch file that adds the location of the SDL2 DLL files to the path environment variable

## Build the ShapeGen demo

Follow these steps to build and run the ShapeGen demo from the command line:

1. Download the latest stable build of SDL2, if you haven't done so already. 
>If you choose a directory other than "C:\SDL2" for your SDL2 installation, you'll need to update the Makefile and sdlpath.bat files in _this_ directory (i.e., the windows-sdl directory in your ShapeGen installation files).
2. Open a Command Window.
3. Run the version of vcvars32.bat in your Visual Studio installation files to initialize the "x86" build environment.
4. Change to _this_ directory (i.e., windows-sdl).
5. Enter the command "nmake" to build the demo.
6. Run the sdlpath.bat file in _this_ directory. This file sets the path for the SDL2 DLL files.
7. Enter the command "demo" to run the demo.

If you need more information, keep reading.

## Downloading SDL2

Download the current stable version of SDL2 from [the official SDL2 download](https://www.libsdl.org/download-2.0.php) page. Create a directory (for example, "C:\SDL2") to copy the SDL2 files and directories to.

Note that the makefile and sdlpath.bat files in _this_ directory (the windows-sdl directory in your ShapeGen installation files) assume that the SDL2 files are located in a directory named "C:\SDL2". If you choose a different directory for your SDL2 installation, be sure to update the Makefile and sdlpath.bat files.

For more information, see the [Installing SDL](https://wiki.libsdl.org/Installation) page at the official SDL2 website.

## Downloading Visual Studio

The makefile in this directory was tested on Windows 10 with the following free version of Visual Studio:
* Microsoft Visual Studio Community 2019, Version 16.1.5

You can download the free version from the Visual Studio [download site](https://visualstudio.microsoft.com/vs/).

## Visual Studio build environment

To build the ShapeGen demo from the command line, you'll first need to open a Command Window. To do this, right-click in the search box next to the Windows Start button, type "command prompt", and press enter.

Next, find the vcvars32.bat file in your Visual Studio installation. Here's the default location for this file:

* C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\

Change to this directory (or to the corresponding location in your Visual Studio installation), and enter the command "vcvars32.bat". This command sets up the Visual Studio build environment.

Next, change to _this_ directory (i.e., the windows-sdl directory in your ShapeGen installation).

Enter the command "nmake.exe". (This command will execute the makefile the same as if you entered the command "nmake.exe /f Makefile".)

After the nmake utility builds the demo.exe executable file, enter the command "sdlpath.bat" to run the batch file in _this_ directory (i.e., windows-sdl). This batch file sets the "path" environment variable so that Windows can find the SDL2 DLL files. 

Next, enter the command "demo" to run the ShapeGen demo.

After the ShapeGen demo starts running, you can press the space bar to advance through the demo.

.

.

.

.
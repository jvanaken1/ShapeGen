Build the ShapeGen demo to run on the Win32 API in Windows
==========================================================

_Author:_ Jerry R. VanAken  
_Date:_ 7/6/2019

This directory (the windows-gdi subdirectory in your ShapeGen installation) contains the files you'll need to build the version of the ShapeGen demo program that runs on the Win32 API in Windows.

The ShapeGen demo runs in a 1280-by-960 window. The demo displays a new image each time you press the space bar. Each image -- including both text and graphics -- is constructed entirely by an instance of the ShapeGen class.

## What's in this directory

This directory (i.e., the windows-gdi subdirectory in your ShapeGen installation) contains these three files:

* `README.md` -- This README file

* `Makefile` -- A makefile that is invoked with the Microsoft nmake utility, and that contains Microsoft Visual Studio C/C++ compiler and linker commands

* `winmain.cpp` -- Contains the platform-specific code necessary to run the ShapeGen demo on the Win32 API in Windows

## Build the ShapeGen demo

Follow these steps to build and run the ShapeGen demo from the command line:

1. Open a Command Window.
2. Run the version of vcvars32.bat in your Visual Studio installation files to initialize the "x86" build environment.
3. Change to _this_ directory (the windows-gdi directory in your ShapeGen installation files).
4. Enter the command "nmake" to build the demo.
5. Enter the command "demo" to run the demo.

If you need more information, keep reading.

## Downloading Visual Studio

The makefile in this directory was tested on Windows 10 with the following free version of Visual Studio:
* Microsoft Visual Studio Community 2019, Version 16.1.5

The download site for Visual Studio is currently "https://visualstudio.microsoft.com/vs/".

## Building with Visual Studio

To build the ShapeGen demo from the command line, you'll first need to open a Command Window. To do this, right-click in the search box next to the Windows Start button, type in "command prompt", and press enter.

Next, find the vcvars32.bat file in your Visual Studio installation. Here's the default location for this file:

* C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\

Change to this directory (or to the corresponding location in your Visual Studio installation), and enter the command "vcvars32.bat". This command sets up the Visual Studio build environment.

Next, change to _this_ directory (i.e., the windows-gdi subdirectory in your ShapeGen installation).

Enter the command "nmake". (This command will execute the makefile the same as if you entered the command "nmake.exe /f Makefile".)

If the nmake utility succeeds in building the demo.exe executable file, enter the command "demo" to run the ShapeGen demo.

After the ShapeGen demo starts running, you can press the space bar to advance through the demo.

.

.

.

.
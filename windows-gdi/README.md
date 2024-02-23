# Build the ShapeGen demo to run on the Win32 API in Windows

_Author:_ Jerry R. VanAken  
_Date:_ 2/22/2024

This directory (the `windows-gdi` subdirectory in your local ShapeGen installation) contains the files you'll need to build the version of the ShapeGen demo program that runs on the Win32 API in Windows.

The ShapeGen demo runs in a 1280-by-960 window. The demo displays a new image each time you press the space bar. Each image -- including both text and graphics -- is constructed entirely by an instance of the ShapeGen class.

## What's in this directory

_This_ directory (the `windows-gdi` subdirectory in your ShapeGen installation) initially contains just these three files:

* `README.md` -- This README file

* `Makefile` -- A makefile that is invoked with the Microsoft `nmake` utility, and that contains Microsoft Visual Studio C/C++ compiler and linker commands

* `winmain.cpp` -- Contains the platform-specific code necessary to run the ShapeGen demo on the Win32 API in Windows

## Build the ShapeGen demo

Follow these steps to build and run the ShapeGen demo from the command line:  
1. Install the free version of Microsoft Visual Studio (see instructions below), if you have not already done so.
2. To open the Command Window that was installed with Visual Studio, open the Windows Start menu. Then scroll down to and click on "Visual Studio 2022", which will display a drop-down list. From this list, select "Developer Command Prompt for VS 2022" (assuming that you will use the same PC to both compile and run the demo).
4. When the Command Window opens, change to _this_ directory (the `windows-gdi` directory in your ShapeGen installation files).
5. Enter the command `nmake` to build the demo program, `demo.exe`.
6. Enter the command `demo` to run the demo program.

The `nmake` command also builds the SVG file viewer, `svgview.exe`. To test `svgview.exe`, you'll need to provide it with a list of one or more SVG files. For more information, see Appendix B in the _ShapeGen User's Guide_ (the `userdoc.pdf` file in this project's main directory).

## Downloading Visual Studio

The makefile in this directory was tested on Windows 10 with the following *free* version of Visual Studio:
* Microsoft Visual Studio Community 2022, Version 17.7.1

The download site for Visual Studio is currently [https://visualstudio.microsoft.com/vs/](https://visualstudio.microsoft.com/vs/)

.

.

.

.

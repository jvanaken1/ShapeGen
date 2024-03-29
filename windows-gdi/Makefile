# Build the ShapeGen demo program to run on the Win32 API in Windows
# This makefile uses the Microsoft C/C++ compiler and linker
# Open a Visual Studio C/C++ command prompt window
# Run the Microsoft nmake utility from the command line in this directory

OBJFILES = winmain.obj alfablur.obj bmpfile.obj textapp.obj gradient.obj pattern.obj\
           renderer.obj arc.obj curve.obj edge.obj path.obj stroke.obj thinline.obj
LIBFILES = user32.lib gdi32.lib Winmm.lib Msimg32.lib
CC = cl.exe
CDEBUG = -Zi
LINK = link.exe
LFLAGS =
LDEBUG = /DEBUG

all : demo.exe svgview.exe

demo.exe : .PHONY demo.obj $(OBJFILES)
        $(LINK) $(LDEBUG) $(LFLAGS) demo.obj $(OBJFILES) $(LIBFILES) /OUT:$@ /PDB:$*.pdb

svgview.exe : .PHONY svgview.obj $(OBJFILES)
        $(LINK) $(LDEBUG) $(LFLAGS) svgview.obj $(OBJFILES) $(LIBFILES) /OUT:$@ /PDB:$*.pdb

# Compile modules for demo program

demo.obj : demo.cpp shapegen.h renderer.h demo.h
        $(CC) $(CDEBUG) -c demo.cpp

svgview.obj : svgview.cpp shapegen.h renderer.h demo.h nanosvg.h
        $(CC) $(CDEBUG) -c svgview.cpp

alfablur.obj : alfablur.cpp shapegen.h renderer.h demo.h
        $(CC) $(CDEBUG) -c alfablur.cpp

bmpfile.obj : bmpfile.cpp shapegen.h renderer.h demo.h
        $(CC) $(CDEBUG) -c bmpfile.cpp

textapp.obj : textapp.cpp shapegen.h renderer.h demo.h
        $(CC) $(CDEBUG) -c textapp.cpp

winmain.obj : winmain.cpp shapegen.h renderer.h demo.h
        $(CC) $(CDEBUG) -c winmain.cpp

# Compile modules for Renderer class

gradient.obj : gradient.cpp shapegen.h renderer.h
        $(CC) $(CDEBUG) -c gradient.cpp

pattern.obj : pattern.cpp shapegen.h renderer.h
        $(CC) $(CDEBUG) -c pattern.cpp

renderer.obj : renderer.cpp shapegen.h renderer.h
        $(CC) $(CDEBUG) -c renderer.cpp

# Compile modules for ShapeGen class

arc.obj : arc.cpp shapegen.h shapepri.h
        $(CC) $(CDEBUG) -c arc.cpp

curve.obj : curve.cpp shapegen.h shapepri.h
        $(CC) $(CDEBUG) -c curve.cpp

edge.obj : edge.cpp shapegen.h shapepri.h
        $(CC) $(CDEBUG) -c edge.cpp

path.obj : path.cpp shapegen.h shapepri.h
        $(CC) $(CDEBUG) -c path.cpp

stroke.obj : stroke.cpp shapegen.h shapepri.h
        $(CC) $(CDEBUG) -c stroke.cpp

thinline.obj : thinline.cpp shapegen.h shapepri.h
        $(CC) $(CDEBUG) -c thinline.cpp

.PHONY :
	@xcopy /d ..\*.cpp
	@xcopy /d ..\*.h
	@xcopy /d ..\data\*.bmp

clean:
        @del *.exe
        @del *.ilk
        @del *.obj
        @del *.pdb



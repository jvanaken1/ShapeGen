# Build the ShapeGen demo program to run on the SDL2 API in Windows
# This makefile uses the Microsoft C/C++ compiler and linker
# Open a Visual Studio C/C++ command prompt window
# Run the Microsoft nmake utility from the command line in this directory
# You may need to edit the include and lib paths for your SDL2 installation
# Before running demo.exe, remember to add C:\SDL2\lib\x86 to your path

INCDIR = C:\SDL2\include
LIBDIR = C:\SDL2\lib\x86
OBJFILES = sdlmain.obj alfablur.obj bmpfile.obj textapp.obj gradient.obj pattern.obj\
           renderer.obj arc.obj curve.obj edge.obj path.obj stroke.obj thinline.obj
LIBFILES = $(LIBDIR)\SDL2main.lib $(LIBDIR)\SDL2.lib shell32.lib
CC = cl.exe
CDEBUG = -Zi
LINK = link.exe
LFLAGS = /SUBSYSTEM:CONSOLE
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

sdlmain.obj : sdlmain.cpp shapegen.h renderer.h demo.h
	$(CC) -I$(INCDIR) $(CDEBUG) -c sdlmain.cpp

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

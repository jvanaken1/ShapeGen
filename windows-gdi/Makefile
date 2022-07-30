# Build ShapeGen demo program to run on the Win32 API in Windows
# This makefile uses the Microsoft C/C++ compiler and linker
# Run the Microsoft nmake utility from the command line in this directory
# Remember to run vcvars32.bat first to set up your build environment

LIBFILES = user32.lib gdi32.lib Winmm.lib Msimg32.lib
OBJFILES = winmain.obj textapp.obj bmpfile.obj pattern.obj gradient.obj renderer.obj\
           arc.obj curve.obj edge.obj path.obj stroke.obj thinline.obj
BMPFILES = bricks.bmp fleur.bmp honeycomb.bmp star.bmp
CC = cl.exe
CDEBUG = -Zi
LINK = link.exe
LFLAGS =
LDEBUG = /DEBUG

all : svgview.exe $(BMPFILES) demo.exe

demo.exe : demo.obj $(OBJFILES)
	$(LINK) $(LDEBUG) $(LFLAGS) $** $(LIBFILES) /OUT:$@ /PDB:$*.pdb

svgview.exe : svgview.obj $(OBJFILES)
	$(LINK) $(LDEBUG) $(LFLAGS) $** $(LIBFILES) /OUT:$@ /PDB:$*.pdb

demo.obj : demo.cpp shapegen.h renderer.h demo.h
	$(CC) $(CDEBUG) -c demo.cpp

svgview.obj : svgview.cpp shapegen.h renderer.h demo.h nanosvg.h
	$(CC) $(CDEBUG) -c svgview.cpp

textapp.obj : textapp.cpp shapegen.h renderer.h demo.h
	$(CC) $(CDEBUG) -c textapp.cpp

bmpfile.obj : bmpfile.cpp shapegen.h renderer.h demo.h
	$(CC) $(CDEBUG) -c bmpfile.cpp

winmain.obj : winmain.cpp shapegen.h renderer.h demo.h
	$(CC) $(CDEBUG) -c winmain.cpp

pattern.obj : pattern.cpp shapegen.h renderer.h
	$(CC) $(CDEBUG) -c pattern.cpp

gradient.obj : gradient.cpp shapegen.h renderer.h
	$(CC) $(CDEBUG) -c gradient.cpp

renderer.obj : renderer.cpp shapegen.h renderer.h
	$(CC) $(CDEBUG) -c renderer.cpp

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

demo.h : ..\demo.h
	copy /y ..\demo.h

shapegen.h : ..\shapegen.h
	copy /y ..\shapegen.h

shapepri.h : ..\shapepri.h
	copy /y ..\shapepri.h

renderer.h : ..\renderer.h
	copy /y ..\renderer.h

nanosvg.h : ..\nanosvg.h
	copy /y ..\nanosvg.h

arc.cpp : ..\arc.cpp
	copy /y ..\arc.cpp

bmpfile.cpp : ..\bmpfile.cpp
	copy /y ..\bmpfile.cpp

curve.cpp : ..\curve.cpp
	copy /y ..\curve.cpp

demo.cpp : ..\demo.cpp
	copy /y ..\demo.cpp

edge.cpp : ..\edge.cpp
	copy /y ..\edge.cpp

gradient.cpp : ..\gradient.cpp
	copy /y ..\gradient.cpp

pattern.cpp : ..\pattern.cpp
	copy /y ..\pattern.cpp

path.cpp : ..\path.cpp
	copy /y ..\path.cpp

renderer.cpp : ..\renderer.cpp
	copy /y ..\renderer.cpp

stroke.cpp : ..\stroke.cpp
	copy /y ..\stroke.cpp

svgview.cpp : ..\svgview.cpp
	copy /y ..\svgview.cpp

textapp.cpp : ..\textapp.cpp
	copy /y ..\textapp.cpp

thinline.cpp : ..\thinline.cpp
	copy /y ..\thinline.cpp

bricks.bmp : ..\bricks.bmp
	copy /y ..\bricks.bmp

fleur.bmp : ..\fleur.bmp
	copy /y ..\fleur.bmp

honeycomb.bmp : ..\honeycomb.bmp
	copy /y ..\honeycomb.bmp

star.bmp : ..\star.bmp
	copy /y ..\star.bmp

clean:
	@del *.exe
	@del *.ilk
	@del *.obj
	@del *.pdb



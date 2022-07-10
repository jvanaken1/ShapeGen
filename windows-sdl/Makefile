# Build ShapeGen demo program to run on SDL2 in Windows
# This makefile uses the Microsoft C/C++ compiler and linker
# Run the Microsoft nmake utility from the command line in this directory
# Remember to run vcvars32.bat first to set up your build environment
# You may need to edit the include and lib paths for your SDL2 installation
# Before running sdldemo.exe, remember to add C:\SDL2\lib\x86 to your path

INCDIR = C:\SDL2\include
LIBDIR = C:\SDL2\lib\x86
LIBFILES = $(LIBDIR)\SDL2main.lib $(LIBDIR)\SDL2.lib shell32.lib
OBJFILES = sdlmain.obj bmpfile.obj textapp.obj gradient.obj pattern.obj \
           arc.obj curve.obj edge.obj path.obj stroke.obj thinline.obj
BMPFILES = bricks.bmp fleur.bmp honeycomb.bmp star.bmp           
CC = cl.exe
CDEBUG = -Zi
LINK = link.exe
LFLAGS = /SUBSYSTEM:CONSOLE
LDEBUG = /DEBUG

all : svgview.exe $(BMPFILES) demo.exe

demo.exe : demo.obj $(OBJFILES)
	$(LINK) $(LDEBUG) $(LFLAGS) demo.obj $(OBJFILES) $(LIBFILES) /OUT:$@ /PDB:$*.pdb
                
svgview.exe : svgview.obj $(OBJFILES)
	$(LINK) $(LDEBUG) $(LFLAGS) $** $(LIBFILES) /OUT:$@ /PDB:$*.pdb

demo.obj : demo.cpp shapegen.h renderer.h demo.h
	$(CC) $(CDEBUG) -c demo.cpp

svgview.obj : svgview.cpp shapegen.h renderer.h demo.h nanosvg.h
	$(CC) $(CDEBUG) -c svgview.cpp

sdlmain.obj : demo.cpp shapegen.h renderer.h demo.h
	$(CC) -I$(INCDIR) $(CDEBUG) -c sdlmain.cpp

bmpfile.obj : bmpfile.cpp shapegen.h renderer.h demo.h
	$(CC) $(CDEBUG) -c bmpfile.cpp

gradient.obj : gradient.cpp shapegen.h renderer.h
	$(CC) $(CDEBUG) -c gradient.cpp

pattern.obj : pattern.cpp shapegen.h renderer.h
	$(CC) $(CDEBUG) -c pattern.cpp

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

textapp.obj : textapp.cpp shapegen.h renderer.h demo.h
	$(CC) $(CDEBUG) -c textapp.cpp

thinline.obj : thinline.cpp shapegen.h shapepri.h
	$(CC) $(CDEBUG) -c thinline.cpp
        
demo.h : ..\demo.h
        copy /y ..\demo.h

renderer.h : ..\renderer.h
        copy /y ..\renderer.h

shapegen.h : ..\shapegen.h
        copy /y ..\shapegen.h

shapepri.h : ..\shapepri.h
        copy /y ..\shapepri.h

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
        
pattern.cpp : ..\pattern.cpp
        copy /y ..\pattern.cpp
        
gradient.cpp : ..\gradient.cpp
        copy /y ..\gradient.cpp
        
path.cpp : ..\path.cpp
        copy /y ..\path.cpp
        
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

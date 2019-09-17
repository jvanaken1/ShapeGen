# Build ShapeGen demo program to run on the Win32 API in Windows
# This makefile uses the Microsoft C/C++ compiler and linker
# Run the Microsoft nmake utility from the command line in this directory
# Remember to run vcvars32.bat first to set up your build environment

LIBFILES = user32.lib gdi32.lib Winmm.lib Msimg32.lib
OBJFILES = demo.obj textapp.obj arc.obj curve.obj edge.obj path.obj stroke.obj thinline.obj
CC = cl.exe
CDEBUG = -Zi
LINK = link.exe
LFLAGS =
LDEBUG = /DEBUG

all : demo.exe

demo.exe : winmain.obj $(OBJFILES)
	$(LINK) $(LDEBUG) $(LFLAGS) $** $(LIBFILES) /OUT:$@ /PDB:$*.pdb

winmain.obj : winmain.cpp demo.h shapegen.h
	$(CC) $(CDEBUG) -c winmain.cpp

demo.obj : demo.cpp demo.h shapegen.h
	$(CC) $(CDEBUG) -c demo.cpp

textapp.obj : textapp.cpp demo.h shapegen.h
	$(CC) $(CDEBUG) -c textapp.cpp

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

arc.cpp : ..\arc.cpp
        copy /y ..\arc.cpp
        
curve.cpp : ..\curve.cpp
        copy /y ..\curve.cpp
        
demo.cpp : ..\demo.cpp
        copy /y ..\demo.cpp
        
edge.cpp : ..\edge.cpp
        copy /y ..\edge.cpp
        
path.cpp : ..\path.cpp
        copy /y ..\path.cpp
        
stroke.cpp : ..\stroke.cpp
        copy /y ..\stroke.cpp
        
textapp.cpp : ..\textapp.cpp
        copy /y ..\textapp.cpp
        
thinline.cpp : ..\thinline.cpp
        copy /y ..\thinline.cpp
        
clean:
        @del *.exe
        @del *.ilk
        @del *.obj
        @del *.pdb
        


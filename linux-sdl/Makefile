# Build ShapeGen demo program to run on SDL2 in Linux
# This makefile uses the GNU C/C++ compiler and linker
# Run the GNU make utility from the command line in this directory

CC = g++

OBJS = demo.o bmpfile.o textapp.o gradient.o pattern.o \
       arc.o curve.o edge.o path.o stroke.o thinline.o

all : .PHONY demo

demo : sdlmain.o $(OBJS)
	$(CC) -o demo sdlmain.o $(OBJS) -lSDL2

demo.o : demo.cpp shapegen.h renderer.h demo.h
	$(CC) -w -c demo.cpp

sdlmain.o : sdlmain.cpp shapegen.h renderer.h
	$(CC) -w -c sdlmain.cpp

bmpfile.o : bmpfile.cpp shapegen.h renderer.h demo.h
	$(CC) -w -c bmpfile.cpp

textapp.o : textapp.cpp shapegen.h renderer.h demo.h
	$(CC) -w -c textapp.cpp

gradient.o : gradient.cpp shapegen.h renderer.h
	$(CC) -w -c gradient.cpp

pattern.o : pattern.cpp shapegen.h renderer.h
	$(CC) -w -c pattern.cpp

arc.o : arc.cpp shapegen.h shapepri.h
	$(CC) -w -c arc.cpp

curve.o : curve.cpp shapegen.h shapepri.h
	$(CC) -w -c curve.cpp

edge.o : edge.cpp shapegen.h shapepri.h
	$(CC) -w -c edge.cpp

path.o : path.cpp shapegen.h shapepri.h
	$(CC) -w -c path.cpp

stroke.o : stroke.cpp shapegen.h shapepri.h
	$(CC) -w -c stroke.cpp

thinline.o : thinline.cpp shapegen.h shapepri.h
	$(CC) -w -c thinline.cpp

.PHONY :
	cp -u ../*.cpp .
	cp -u ../*.h .
	cp -u ../*.bmp .

clean :
	rm *.o
	rm demo


/*
  Copyright (C) 2019-2023 Jerry R. VanAken

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.

  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

  3. This notice may not be removed or altered from any source distribution.
*/
//---------------------------------------------------------------------
//
// textapp.cpp:
//   All of the text in the ShapeGen demo is drawn by this simple,
//   ShapeGen-based graphical text application. This file mostly
//   consists of font data for the printable ASCII characters. The
//   font is stroke-based and is constructed using a combination of
//   line segments, ellipses, elliptic arcs, and elliptic splines.
//   The TextApp class member functions are implemented near the end
//   of this file.
//
//---------------------------------------------------------------------

#include <string.h>
#include <math.h>
#include <assert.h>
#include "demo.h"

struct GLYPH
{
    int charcode;  // character code for this glyph
    int xyindex;   // glyph's starting index into xytbl array
    int xylen;     // number of xytbl elements for this glyph
    SGPoint *xy;   // transformed glyph x-y coords in 16.16 fixed point
    char *displist;  // string containing glyph display list
};

// Display list verbs
#define MOVE                "\x1"
#define LINE                "\x2"
#define CLOSEFIGURE         "\x3"
#define ENDFIGURE           "\x4"
#define POLYLINE            "\x5"
#define ELLIPSE             "\x6"
#define ELLIPTICARC         "\x7"
#define POLYELLIPTICSPLINE  "\x8"
#define DRAWADOT            "\x9"

const int _MOVE_               = 0x1;
const int _LINE_               = 0x2;
const int _CLOSEFIGURE_        = 0x3;
const int _ENDFIGURE_          = 0x4;
const int _POLYLINE_           = 0x5;
const int _ELLIPSE_            = 0x6;
const int _ELLIPTICARC_        = 0x7;
const int _POLYELLIPTICSPLINE_ = 0x8;
const int _DRAWADOT_           = 0x9;

// Right- and left-side bearings for this font
const float leftbearing = 8.0;
const float rightbearing = 8.0;

//---------------------------------------------------------------------
//
// The xytbl array contains the points that define the shapes of all
// the glyphs. The first two entries for each glyph specify the
// bounding box for the glyph.
//
// Font metrics (specified in pixels):
//   - Glyph-relative x-y origin is located at intersection of baseline
//     with left edge of bounding box
//   - Ascender line is at y = 66
//   - Descender line is at y = -26
//   - Caps height is 60
//   - x height is 42
//   - M width is 64
//
//---------------------------------------------------------------------

const XY xytbl[] = {

    // glyph ' ' (blank space)
    {  0.000,  0.000 },  // 0 bounding box min
    { 16.000, 60.000 },  // 1 bounding box max

    // glyph '!'
    {  0.000,  0.000 },  // 0 bounding box min
    {  4.000, 66.000 },  // 1 bounding box max
    {  2.000,  0.000 },  // 2
    {  2.000, 66.000 },  // 3
    {  2.000, 19.000 },  // 4

    // glyph '\"'
    {  0.000, 54.000 },  //  0 bounding box min
    { 20.000, 66.000 },  //  1 bounding box max
    {  4.000, 66.000 },  //  2
    {  4.000, 54.000 },  //  3
    { 16.000, 66.000 },  //  4
    { 16.000, 54.000 },  //  5

    // glyph '#'
    {  0.000,  0.000 },  // 0 bounding box min
    { 49.000, 60.000 },  // 1 bounding box max
    { 17.500, 60.000 },  // 2
    {  9.500,  0.000 },  // 3
    { 39.500, 60.000 },  // 4
    { 31.500,  0.000 },  // 5
    {  3.500, 40.000 },  // 6
    { 49.000, 40.000 },  // 7
    {  0.000, 19.000 },  // 8
    { 45.500, 19.000 },  // 9

    // glyph '$'
    {  0.000, -7.000 },  //  0 bounding box min
    { 32.000, 60.000 },  //  1 bounding box max
    { 29.325, 52.404 },  //  2
    { 24.454, 59.957 },  //  3
    { 16.048, 59.957 },  //  4
    {  2.675, 59.957 },  //  5
    {  2.675, 47.369 },  //  6
    {  2.675, 34.277 },  //  7
    { 16.239, 32.464 },  //  8
    { 32.000, 30.248 },  //  9
    { 32.000, 16.451 },  // 10
    { 32.000,  1.043 },  // 11
    { 16.239,  1.043 },  // 12
    {  4.777,  1.043 },  // 13
    {  0.000, 12.121 },  // 14
    { 16.000, -7.000 },  // 15
    { 16.000, 66.000 },  // 16

    // glyph '%'
    {  0.000,  0.000 },  // 0 bounding box min
    { 60.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    { 60.000, 60.000 },  // 3
    { 10.000, 45.000 },  // 4
    { 10.000, 60.000 },  // 5
    { 20.000, 45.000 },  // 6
    { 50.000, 15.000 },  // 7
    { 50.000, 30.000 },  // 8
    { 60.000, 15.000 },  // 9

    // glyph '&'
    {  0.000, -1.000 },  //  0 bounding box min
    { 46.000, 61.000 },  //  1 bounding box max
    { 43.121, 30.448 },  //  2
    { 43.121, -1.000 },  //  3
    { 17.411, -1.000 },  //  4
    {  0.000, -1.000 },  //  5
    {  0.000, 15.127 },  //  6
    {  0.000, 21.309 },  //  7
    {  4.691, 25.879 },  //  8
    {  8.390, 30.090 },  //  9
    { 16.960, 33.494 },  // 10
    { 26.251, 38.064 },  // 11
    { 29.950, 42.185 },  // 12
    { 33.107, 45.142 },  // 13
    { 33.107, 49.711 },  // 14
    { 33.198, 61.000 },  // 15
    { 20.839, 61.000 },  // 16
    {  7.939, 61.000 },  // 17
    {  7.939, 49.084 },  // 18
    {  7.939, 44.156 },  // 19
    { 11.367, 39.766 },  // 20
    { 16.148, 32.777 },  // 21
    { 47.000,  0.000 },  // 22

    // glyph '\''
    {  0.000, 54.000 },  // 0 bounding box min
    {  8.000, 66.000 },  // 1 bounding box max
    {  4.000, 66.000 },  // 2
    {  4.000, 54.000 },  // 3

    // glyph '('
    {  0.000,-26.000 },  // 0 bounding box min
    { 14.000, 66.000 },  // 1 bounding box max
    { 14.000, 66.000 },  // 2
    {  0.000, 50.000 },  // 3
    {  0.000, 20.000 },  // 4
    {  0.000,-10.000 },  // 5
    { 14.000,-26.000 },  // 6

    // glyph ')'
    {  0.000,-26.000 },  // 0 bounding box min
    { 14.000, 66.000 },  // 1 bounding box max
    {  0.000, 66.000 },  // 2
    { 14.000, 50.000 },  // 3
    { 14.000, 20.000 },  // 4
    { 14.000,-10.000 },  // 5
    {  0.000,-26.000 },  // 6

    // glyph '*'
    {  0.000, 26.000 },  // 0 bounding box min
    { 34.000, 66.000 },  // 1 bounding box max
    { 17.000, 66.000 },  // 2
    { 17.000, 26.000 },  // 3
    { 34.000, 56.000 },  // 4
    {  0.000, 36.000 },  // 5
    { 34.000, 36.000 },  // 6
    {  0.000, 56.000 },  // 7

    // glyph '+'
    {  0.000,  4.000 },  // 0 bounding box min
    { 42.000, 46.000 },  // 1 bounding box max
    { 21.000, 46.000 },  // 2
    { 21.000,  4.000 },  // 3
    {  0.000, 25.000 },  // 4
    { 42.000, 25.000 },  // 5

    // glyph ','
    {  0.000, -9.000 },  // 0 bounding box min
    {  5.000,  0.000 },  // 1 bounding box max
    {  2.500,  0.000 },  // 2
    {  5.000, -1.000 },  // 3
    {  5.000, -5.000 },  // 4
    {  0.000,-12.000 },  // 5

    // glyph '-'
    {  0.000, 21.000 },  // 0 bounding box min
    { 32.000, 23.000 },  // 1 bounding box max
    {  0.000, 22.000 },  // 4
    { 32.000, 22.000 },  // 5

    // glyph '.'
    {  0.000,  0.000 },  // 0 bounding box min
    {  4.000,  0.000 },  // 1 bounding box max
    {  2.000,  0.000 },  // 2

    // glyph '/'
    {  0.000,-26.000 },  // 0 bounding box min
    { 36.000, 66.000 },  // 1 bounding box max
    {  0.000,-26.000 },  // 2
    { 36.000, 66.000 },  // 3

    // glyph '0' (numeral zero)
    {  0.000,  0.000 },  // 0 bounding box min
    { 42.000, 61.000 },  // 1 bounding box max
    { 21.000, 30.000 },  // 2
    { 21.000, 61.000 },  // 3
    { 42.000, 31.000 },  // 4

    // glyph '1'
    {  0.000,  0.000 },  // 0 bounding box min
    { 16.000, 60.000 },  // 1 bounding box max
    { 12.000,  0.000 },  // 2
    { 12.000, 60.000 },  // 3
    {  4.000, 44.000 },  // 4

    // glyph '2'
    {  0.000,  0.000 },  // 0 bounding box min
    { 38.000, 61.000 },  // 1 bounding box max
    { 17.500, 43.000 },  // 2
    { 26.500, 58.700 },  // 3
    { 33.400, 34.300 },  // 4
    { 28.600, 25.800 },  // 5
    { 18.000, 20.000 },  // 6
    {  0.000, 10.000 },  // 7
    {  0.000,  0.000 },  // 8
    { 38.000,  0.000 },  // 9

    // glyph '3'
    {  0.000, -1.000 },  //  0 bounding box min
    { 38.000, 61.000 },  //  1 bounding box max
    {  3.000, 55.000 },  //  2
    {  8.000, 61.000 },  //  3
    { 20.000, 61.000 },  //  4
    { 36.000, 61.000 },  //  5
    { 36.000, 47.500 },  //  6
    { 36.000, 33.000 },  //  7
    { 21.000, 33.000 },  //  8
    {  8.000, 33.000 },  //  9
    { 21.000, 33.000 },  // 10
    { 38.000, 33.000 },  // 11
    { 38.000, 17.000 },  // 12
    { 38.000, -1.000 },  // 13
    { 20.000, -1.000 },  // 14
    {  8.000, -1.000 },  // 15
    {  0.000,  7.000 },  // 16

    // glyph '4'
    {  0.000,  0.000 },  //  0 bounding box min
    { 40.000, 60.000 },  //  1 bounding box max
    { 32.000,  0.000 },  //  2
    { 32.000, 60.000 },  //  3
    {  0.000, 17.000 },  //  4
    { 40.000, 17.000 },  //  5

    // glyph '5'
    {  0.000, -1.000 },  //  0 bounding box min
    { 38.000, 60.000 },  //  1 bounding box max
    { 34.000, 60.000 },  //  2
    {  3.000, 60.000 },  //  3
    {  3.000, 32.000 },  //  4
    { 11.000, 35.000 },  //  5
    { 21.000, 35.000 },  //  6
    { 38.000, 35.000 },  //  7
    { 38.000, 17.000 },  //  8
    { 38.000, -1.000 },  //  9
    { 20.000, -1.000 },  // 10
    {  8.000, -1.000 },  // 11
    {  0.000,  7.000 },  // 12

    // glyph '6'
    {  0.000, -1.000 },  //  0 bounding box min
    { 38.000, 61.000 },  //  1 bounding box max
    { 20.000, 17.500 },  //  2
    {  2.700, 11.000 },  //  3
    { 13.500, 34.800 },  //  4
    {  2.700, 11.000 },  //  5
    { -4.800, 30.000 },  //  6
    {  2.700, 49.000 },  //  7
    { 20.000, 42.500 },  //  8
    {  2.700, 49.000 },  //  9
    { 26.500, 59.800 },  // 10

    // glyph '7'
    {  0.000,  0.000 },  //  0 bounding box min
    { 44.000, 60.000 },  //  1 bounding box max
    {  0.000, 60.000 },  //  2
    { 44.000, 60.000 },  //  3
    { 22.000, 50.000 },  //  4
    { 14.000,  0.000 },  //  5

    // glyph '8'
    {  0.000, -1.000 },  //  0 bounding box min
    { 38.000, 61.000 },  //  1 bounding box max
    { 19.000, 32.300 },  //  2
    { 35.000, 34.300 },  //  3
    { 35.000, 48.500 },  //  4
    { 35.000, 61.000 },  //  5
    { 19.000, 61.000 },  //  6
    {  3.000, 61.000 },  //  7
    {  3.000, 48.500 },  //  8
    {  3.000, 34.300 },  //  9
    { 19.000, 32.300 },  // 10
    { 38.000, 30.000 },  // 11
    { 38.000, 15.000 },  // 12
    { 38.000, -1.000 },  // 13
    { 19.000, -1.000 },  // 14
    {  0.000, -1.000 },  // 15
    {  0.000, 15.000 },  // 16
    {  0.000, 30.000 },  // 17
    { 19.000, 32.300 },  // 18

    // glyph '9'
    {  0.000, -1.000 },  //  0 bounding box min
    { 38.000, 61.000 },  //  1 bounding box max
    { 18.000, 42.500 },  // 2
    { 35.300, 49.000 },  // 3
    { 24.500, 25.200 },  // 4
    { 35.300, 49.000 },  // 5
    { 42.800, 30.000 },  // 6
    { 35.300, 11.000 },  // 7
    { 18.000, 17.500 },  // 8
    { 35.300, 11.000 },  // 9
    { 11.500,  0.200 },  // 10

    // glyph ':'
    {  0.000,  0.000 },  // 0 bounding box min
    {  4.000, 42.000 },  // 1 bounding box max
    {  2.000, 42.000 },  // 2
    {  2.000,  0.000 },  // 3

    // glyph ';'
    {  0.000, -9.000 },  // 0 bounding box min
    {  5.000,  0.000 },  // 1 bounding box max
    {  2.500,  0.000 },  // 2
    {  5.000, -1.000 },  // 3
    {  5.000, -5.000 },  // 4
    {  0.000,-12.000 },  // 5
    {  2.500, 42.000 },  // 6

    // glyph '<'
    {  0.000, 10.000 },  //  0 bounding box min
    { 38.000, 46.000 },  //  1 bounding box max
    { 38.000, 46.000 },  //  2
    {  0.000, 28.000 },  //  3
    { 38.000, 10.000 },  //  4

    // glyph '='
    {  0.000, 18.000 },  //  0 bounding box min
    { 35.000, 38.000 },  //  1 bounding box max
    {  0.000, 38.000 },  //  2
    { 35.000, 38.000 },  //  3
    {  0.000, 18.000 },  //  4
    { 35.000, 18.000 },  //  5

    // glyph '>'
    {  0.000, 10.000 },  //  0 bounding box min
    { 38.000, 46.000 },  //  1 bounding box max
    {  0.000, 46.000 },  //  2
    { 38.000, 28.000 },  //  3
    {  0.000, 10.000 },  //  4

    // glyph '?'
    {  0.000,  0.000 },  //  0 bounding box min
    { 35.000, 66.000 },  //  1 bounding box max
    {  0.000, 47.025 },  //  2
    {  0.000, 66.000 },  //  3
    { 18.261, 66.000 },  //  4
    { 35.000, 66.000 },  //  5
    { 35.000, 50.531 },  //  6
    { 35.000, 44.962 },  //  7
    { 32.312, 40.941 },  //  8
    { 29.623, 36.919 },  //  9
    { 25.565, 33.980 },  // 10
    { 21.507, 31.041 },  // 11
    { 19.123, 26.245 },  // 12
    { 16.739, 21.450 },  // 13
    { 17.246, 14.438 },  // 14
    { 17.246,  0.000 },  // 15

    // glyph '@'
    {  0.000, -8.000 },  //  0 bounding box min
    { 68.000, 60.000 },  //  1 bounding box max
    { 32.286, 26.000 },  //  2
    { 37.517, 42.564 },  //  3
    { 45.093, 22.338 },  //  4
    { 47.077, 42.041 },  //  5
    { 42.387, 10.133 },  //  6
    { 55.013,  7.344 },  //  7
    { 68.000,  4.205 },  //  8
    { 68.000, 28.615 },  //  9
    { 68.000, 60.000 },  // 10
    { 34.812, 60.000 },  // 11
    {  0.000, 60.000 },  // 12
    {  0.000, 23.385 },  // 13
    {  0.000, -8.000 },  // 14
    { 29.761, -8.000 },  // 15
    { 39.682, -8.000 },  // 16
    { 45.273, -5.385 },  // 17

    // glyph 'A'
    {  0.000,  0.000 },  // 0 bounding box min
    { 50.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    { 25.000, 60.000 },  // 3
    { 50.000,  0.000 },  // 4
    {  9.167, 22.000 },  // 5
    { 40.833, 22.000 },  // 6

    // glyph 'B'
    {  0.000,  0.000 },  //  0 bounding box min
    { 37.000, 60.000 },  //  1 bounding box max
    {  0.000, 60.000 },  //  2
    { 20.500, 60.000 },  //  3
    { 34.000, 60.000 },  //  4
    { 34.000, 46.500 },  //  5
    { 34.000, 33.000 },  //  6
    { 20.500, 33.000 },  //  7
    {  0.000, 33.000 },  //  8
    { 20.500, 33.000 },  //  9
    { 37.000, 33.000 },  // 10
    { 37.000, 16.500 },  // 11
    { 37.000,  0.000 },  // 12
    { 20.500,  0.000 },  // 13
    {  0.000,  0.000 },  // 14

    // glyph 'C'
    {  0.000, -1.000 },  // 0 bounding box min
    { 47.800, 61.000 },  // 1 bounding box max
    { 28.000, 30.000 },  // 2
    { 28.000, -1.000 },  // 3
    {  0.000, 30.000 },  // 4

    // glyph 'D'
    {  0.000,  0.000 },  // 0 bounding box min
    { 42.000, 60.000 },  // 1 bounding box max
    {  0.000, 60.000 },  // 2
    { 16.000, 60.000 },  // 3
    { 42.000, 60.000 },  // 4
    { 42.000, 30.000 },  // 5
    { 42.000,  0.000 },  // 6
    { 16.000,  0.000 },  // 7
    {  0.000,  0.000 },  // 8

    // glyph 'E'
    {  0.000,  0.000 },  // 0 bounding box min
    { 34.000, 60.000 },  // 1 bounding box max
    { 34.000,  0.000 },  // 2
    {  0.000,  0.000 },  // 3
    {  0.000, 60.000 },  // 4
    { 34.000, 60.000 },  // 5
    {  0.000, 33.000 },  // 6
    { 29.000, 33.000 },  // 7

    // glyph 'F'
    {  0.000,  0.000 },  // 0 bounding box min
    { 34.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    {  0.000, 60.000 },  // 3
    { 34.000, 60.000 },  // 4
    {  0.000, 33.000 },  // 5
    { 29.000, 33.000 },  // 6

    // glyph 'G'
    {  0.000, -1.000 },  // 0 bounding box min
    { 47.800, 61.000 },  // 1 bounding box max
    { 28.000, 30.000 },  // 2
    { 28.000, -1.000 },  // 3
    {  0.000, 30.000 },  // 4
    { 34.000, 24.000 },  // 5
    { 47.800, 24.000 },  // 6
    { 47.800,  0.000 },  // 7

    // glyph 'H'
    {  0.000,  0.000 },  // 0 bounding box min
    { 40.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    {  0.000, 60.000 },  // 3
    { 40.000,  0.000 },  // 4
    { 40.000, 60.000 },  // 5
    {  0.000, 32.000 },  // 6
    { 40.000, 32.000 },  // 7

    // glyph 'I'
    {  0.000,  0.000 },  // 0 bounding box min
    {  4.000, 60.000 },  // 1 bounding box max
    {  2.000,  0.000 },  // 2
    {  2.000, 60.000 },  // 3

    // glyph 'J'
    {  0.000, -1.000 },  // 0 bounding box min
    { 38.000, 60.000 },  // 1 bounding box max
    {  0.000, 12.930 },  // 2
    {  4.471,  2.186 },  // 3
    { 16.765, -0.500 },  // 4
    { 38.000, -5.000 },  // 5
    { 38.000, 15.000 },  // 6
    { 38.000, 60.000 },  // 7

    // glyph 'K'
    {  0.000,  0.000 },  // 0 bounding box min
    { 38.000, 60.000 },  // 1 bounding box max
    { 38.000,  0.000 },  // 2
    {  9.000, 35.000 },  // 3
    {  0.000, 32.000 },  // 4
    {  8.200, 30.500 },  // 5
    { 35.000, 60.000 },  // 6
    {  0.000,  0.000 },  // 7
    {  0.000, 60.000 },  // 8

    // glyph 'L'
    {  0.000,  0.000 },  // 0 bounding box min
    { 34.000, 60.000 },  // 1 bounding box max
    {  0.000, 60.000 },  // 2
    {  0.000,  0.000 },  // 3
    { 34.000,  0.000 },  // 4

    // glyph 'M'
    {  0.000,  0.000 },  // 0 bounding box min
    { 64.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    {  0.000, 60.000 },  // 3
    { 32.000, 10.000 },  // 4
    { 64.000, 60.000 },  // 5
    { 64.000,  0.000 },  // 6

    // glyph 'N'
    {  0.000,  0.000 },  // 0 bounding box min
    { 40.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    {  0.000, 60.000 },  // 3
    { 40.000,  0.000 },  // 4
    { 40.000, 60.000 },  // 5

    // glyph 'O'
    {  0.000, -1.000 },  // 0 bounding box min
    { 56.000, 61.000 },  // 1 bounding box max
    { 28.000, 30.000 },  // 2
    { 28.000, -1.000 },  // 3
    {  0.000, 30.000 },  // 4

    // glyph 'P'
    {  0.000,  0.000 },  // 0 bounding box min
    { 37.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    {  0.000, 60.000 },  // 3
    { 20.500, 60.000 },  // 4
    { 37.000, 60.000 },  // 5
    { 37.000, 43.500 },  // 6
    { 37.000, 27.000 },  // 7
    { 20.500, 27.000 },  // 8
    {  0.000, 27.000 },  // 9

    // glyph 'Q'
    {  0.000, -3.000 },  // 0 bounding box min
    { 56.000, 61.000 },  // 1 bounding box max
    { 28.000, 30.000 },  // 2
    { 28.000, -1.000 },  // 3
    {  0.000, 30.000 },  // 4
    { 35.000, 14.000 },  // 5
    { 35.000, -3.000 },  // 6
    { 54.000, -3.000 },  // 7

    // glyph 'R'
    {  0.000,  0.000 },  // 0 bounding box min
    { 38.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    {  0.000, 60.000 },  // 3
    { 15.000, 60.000 },  // 4
    { 33.000, 60.000 },  // 5
    { 33.000, 45.000 },  // 6
    { 33.000, 30.000 },  // 7
    { 15.000, 30.000 },  // 8
    {  0.000, 30.000 },  // 9
    { 10.000, 30.000 },  // 10
    { 20.000, 30.000 },  // 11
    { 38.000,  0.000 },  // 12

    // glyph 'S'
    {  0.000, -1.000 },  //  0 bounding box min
    { 38.000, 61.000 },  //  1 bounding box max
    { 34.824, 53.051 },  //  2
    { 29.039, 61.000 },  //  3
    { 19.057, 61.000 },  //  4
    {  3.176, 61.000 },  //  5
    {  3.176, 47.752 },  //  6
    {  3.176, 33.974 },  //  7
    { 19.284, 32.067 },  //  8
    { 38.000, 29.735 },  //  9
    { 38.000, 15.215 },  // 10
    { 38.000, -1.000 },  // 11
    { 19.284, -1.000 },  // 12
    {  5.672, -1.000 },  // 13
    {  0.000, 10.658 },  // 14

    // glyph 'T'
    {  0.000,  0.000 },  // 0 bounding box min
    { 42.000, 60.000 },  // 1 bounding box max
    {  0.000, 60.000 },  // 2
    { 42.000, 60.000 },  // 3
    { 21.000, 60.000 },  // 4
    { 21.000,  0.000 },  // 5

    // glyph 'U'
    {  0.000, -1.000 },  // 0 bounding box min
    { 40.000, 60.000 },  // 1 bounding box max
    {  0.000, 60.000 },  // 2
    {  0.000, 20.500 },  // 3
    {  0.000, -1.000 },  // 4
    { 20.000, -1.000 },  // 5
    { 40.000, -1.000 },  // 6
    { 40.000, 20.500 },  // 7
    { 40.000, 60.000 },  // 8

    // glyph 'V'
    {  0.000, 0.0000 },  // 0 bounding box min
    { 44.000, 60.000 },  // 1 bounding box max
    {  0.000, 60.000 },  // 2
    { 22.000,  0.000 },  // 3
    { 44.000, 60.000 },  // 4

    // glyph 'W'
    {  0.000, 0.0000 },  // 0 bounding box min
    { 76.000, 60.000 },  // 1 bounding box max
    {  0.000, 60.000 },  // 2
    { 19.000,  0.000 },  // 3
    { 38.000, 60.000 },  // 4
    { 57.000,  0.000 },  // 5
    { 76.000, 60.000 },  // 6

    // glyph 'X'
    {  0.000, 0.0000 },  // 0 bounding box min
    { 44.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    { 42.000, 60.000 },  // 3
    {  2.000, 60.000 },  // 4
    { 44.000,  0.000 },  // 5

    // glyph 'Y'
    {  0.000, 0.0000 },  // 0 bounding box min
    { 44.000, 60.000 },  // 1 bounding box max
    {  0.000, 60.000 },  // 2
    { 22.000, 26.000 },  // 3
    { 44.000, 60.000 },  // 4
    { 22.000, 26.000 },  // 5
    { 22.000,  0.000 },  // 6

    // glyph 'Z'
    {  0.000, 0.0000 },  // 0 bounding box min
    { 40.000, 60.000 },  // 1 bounding box max
    {  2.000, 60.000 },  // 2
    { 38.000, 60.000 },  // 3
    {  0.000,  0.000 },  // 4
    { 40.000,  0.000 },  // 5

    // glyph '['
    {  0.000,-26.000 },  // 0 bounding box min
    { 14.000, 66.000 },  // 1 bounding box max
    { 14.000, 66.000 },  // 2
    {  0.000, 66.000 },  // 3
    {  0.000,-26.000 },  // 4
    { 14.000,-26.000 },  // 5

    // glyph '\\'
    {  0.000,-26.000 },  // 0 bounding box min
    { 36.000, 66.000 },  // 1 bounding box max
    { 36.000,-26.000 },  // 2
    {  0.000, 66.000 },  // 3

    // glyph ']'
    {  0.000,-26.000 },  // 0 bounding box min
    { 14.000, 66.000 },  // 1 bounding box max
    {  0.000, 66.000 },  // 2
    { 14.000, 66.000 },  // 3
    { 14.000,-26.000 },  // 4
    {  0.000,-26.000 },  // 5

    // glyph '^'
    {  0.000, 24.000 },  // 0 bounding box min
    { 34.000, 60.000 },  // 1 bounding box max
    {  0.000, 24.000 },  // 2
    { 17.000, 60.000 },  // 3
    { 34.000, 24.000 },  // 4

    // glyph '_'
    {  0.000,-26.000 },  // 0 bounding box min
    { 42.000,-26.000 },  // 1 bounding box max
    {  0.000,-26.000 },  // 2
    { 42.000,-26.000 },  // 3

    // glyph '`'
    {  0.000, 54.000 },  // 0 bounding box min
    { 11.000, 66.000 },  // 1 bounding box max
    {  0.000, 66.000 },  // 2
    { 11.000, 54.000 },  // 3

    // glyph 'a'
    {  0.000, -1.000 },  // 0 bounding box min
    { 40.000, 43.000 },  // 1 bounding box max
    { 20.000, 21.000 },  // 2
    { 40.000, 21.000 },  // 4
    { 20.000, -1.000 },  // 3
    { 40.000, 42.000 },  // 5
    { 40.000,  0.000 },  // 6

    // glyph 'b'
    {  0.000, -1.000 },  // 0 bounding box min
    { 40.000, 43.000 },  // 1 bounding box max
    { 20.000, 21.000 },  // 2
    { 20.000, -1.000 },  // 3
    {  0.000, 21.000 },  // 4
    {  0.000, 66.000 },  // 5
    {  0.000,  0.000 },  // 6

    // glyph 'c'
    {  0.000, -1.000 },  // 0 bounding box min
    { 34.200, 43.000 },  // 1 bounding box max
    { 20.000, 21.000 },  // 2
    { 20.000, -1.000 },  // 3
    {  0.000, 21.000 },  // 4

    // glyph 'd'
    {  0.000, -1.000 },  // 0 bounding box min
    { 40.000, 43.000 },  // 1 bounding box max
    { 20.000, 21.000 },  // 2
    { 40.000, 21.000 },  // 4
    { 20.000, -1.000 },  // 3
    { 40.000, 66.000 },  // 5
    { 40.000,  0.000 },  // 6

    // glyph 'e'
    {  0.000, -1.000 },  //  0 bounding box min
    { 40.000, 43.000 },  //  1 bounding box max
    {  0.000, 22.000 },  //  2
    { 40.000, 22.000 },  //  3
    { 40.000, 43.000 },  //  4
    { 20.000, 43.000 },  //  5
    {  0.000, 43.000 },  //  6
    {  0.000, 22.000 },  //  7
    {  0.000, -6.000 },  //  8
    { 26.000, -1.000 },  //  9
    { 33.000,  0.000 },  // 10
    { 38.000,  3.000 },  // 11

    // glyph 'f'
    {  0.000,  0.000 },  // 0 bounding box min
    { 38.000, 66.000 },  // 1 bounding box max
    { 38.000, 59.850 },  // 2
    { 34.941, 64.734 },  // 3
    { 26.529, 65.955 },  // 4
    { 12.000, 68.000 },  // 5
    { 12.000, 58.455 },  // 6
    { 12.000,  0.000 },  // 7
    {  0.000, 42.000 },  // 8
    { 32.000, 42.000 },  // 9

    // glyph 'g'
    {  0.000,-26.000 },  //  0 bounding box min
    { 40.000, 43.000 },  //  1 bounding box max
    { 20.000, 21.000 },  //  2
    { 40.000, 21.000 },  //  4
    { 20.000, -1.000 },  //  3
    { 40.000, 42.000 },  //  5
    { 40.000, -8.400 },  //  6
    { 40.000,-30.400 },  //  7
    { 17.000,-25.400 },  //  8
    {  9.000,-23.400 },  //  9
    {  4.000,-17.400 },  // 10

    // glyph 'h'
    {  0.000,  0.000 },  // 0 bounding box min
    { 34.000, 43.000 },  // 1 bounding box max
    {  0.000, 66.000 },  // 2
    {  0.000,  0.000 },  // 3
    {  0.000, 28.000 },  // 4
    {  4.000, 40.000 },  // 5
    { 15.000, 43.000 },  // 6
    { 34.000, 47.000 },  // 7
    { 34.000, 28.000 },  // 8
    { 34.000,  0.000 },  // 9

    // glyph 'i'
    {  0.000,  0.000 },  // 0 bounding box min
    {  4.000, 60.000 },  // 1 bounding box max
    {  2.000,  0.000 },  // 2
    {  2.000, 42.000 },  // 3
    {  2.000, 60.000 },  // 4

    // glyph 'j'
    {  0.000,-26.000 },  // 0 bounding box min
    { 17.000, 60.000 },  // 1 bounding box max
    { 16.000, 42.000 },  // 2
    { 16.000,-16.100 },  // 3
    { 16.000,-28.500 },  // 4
    {  4.000,-25.700 },  // 5
    {  2.000,-25.300 },  // 6
    {  0.000,-24.300 },  // 7
    { 16.000, 60.000 },  // 8

    // glyph 'k'
    {  0.000,  0.000 },  // 0 bounding box min
    { 30.000, 66.000 },  // 1 bounding box max
    {  0.000, 66.000 },  // 2
    {  0.000,  0.000 },  // 3
    {  0.000, 12.000 },  // 4
    { 30.000, 42.000 },  // 5
    {  9.000, 21.000 },  // 6
    { 30.000,  0.000 },  // 7

    // glyph 'l'
    {  0.000,  0.000 },  // 0 bounding box min
    {  4.000, 66.000 },  // 1 bounding box max
    {  2.000,  0.000 },  // 2
    {  2.000, 66.000 },  // 3

    // glyph 'm'
    {  0.000,  0.000 },  //  0 bounding box min
    { 54.000, 43.000 },  //  1 bounding box max
    {  0.000, 42.000 },  //  2
    {  0.000,  0.000 },  //  3
    {  0.000, 28.000 },  //  4
    {  3.176, 40.000 },  //  5
    { 11.912, 43.000 },  //  6
    { 27.000, 47.000 },  //  7
    { 27.000, 28.000 },  //  8
    { 27.000,  0.000 },  //  9
    { 27.000, 28.000 },  // 10
    { 30.176, 40.000 },  // 11
    { 38.912, 43.000 },  // 12
    { 54.000, 47.000 },  // 13
    { 54.000, 28.000 },  // 14
    { 54.000,  0.000 },  // 15

    // glyph 'n'
    {  0.000,  0.000 },  // 0 bounding box min
    { 34.000, 43.000 },  // 1 bounding box max
    {  0.000, 42.000 },  // 2
    {  0.000,  0.000 },  // 3
    {  0.000, 28.000 },  // 4
    {  4.000, 40.000 },  // 5
    { 15.000, 43.000 },  // 6
    { 34.000, 47.000 },  // 7
    { 34.000, 28.000 },  // 8
    { 34.000,  0.000 },  // 9

    // glyph 'o'
    {  0.000, -1.000 },  // 0 bounding box min
    { 40.000, 43.000 },  // 1 bounding box max
    { 20.000, 21.000 },  // 2
    { 20.000, -1.000 },  // 3
    {  0.000, 21.000 },  // 4

    // glyph 'p'
    {  0.000,-26.000 },  // 0 bounding box min
    { 40.000, 43.000 },  // 1 bounding box max
    { 20.000, 21.000 },  // 2
    { 20.000, -1.000 },  // 3
    {  0.000, 21.000 },  // 4
    {  0.000, 42.000 },  // 5
    {  0.000,-26.000 },  // 6

    // glyph 'q'
    {  0.000,-26.000 },  // 0 bounding box min
    { 40.000, 43.000 },  // 1 bounding box max
    { 20.000, 21.000 },  // 2
    { 40.000, 21.000 },  // 4
    { 20.000, -1.000 },  // 3
    { 40.000, 42.000 },  // 5
    { 40.000,-26.000 },  // 6

    // glyph 'r'
    {  0.000,  0.000 },  // 0 bounding box min
    { 34.000, 43.000 },  // 1 bounding box max
    {  0.000, 42.000 },  // 2
    {  0.000,  0.000 },  // 3
    {  0.000, 28.000 },  // 4
    {  4.000, 40.000 },  // 5
    { 15.000, 43.000 },  // 6
    { 34.000, 47.000 },  // 7
    { 34.000, 34.000 },  // 8

    // glyph 's'
    {  0.000, -1.000 },  //  0 bounding box min
    { 34.000, 43.000 },  //  1 bounding box max
    { 29.325, 37.359 },  //  2
    { 24.454, 43.000 },  //  3
    { 16.048, 43.000 },  //  4
    {  2.675, 43.000 },  //  5
    {  2.675, 33.598 },  //  6
    {  2.675, 23.821 },  //  7
    { 16.239, 22.467 },  //  8
    { 32.000, 20.812 },  //  9
    { 32.000, 10.508 },  // 10
    { 32.000, -1.000 },  // 11
    { 16.239, -1.000 },  // 12
    {  4.776, -1.000 },  // 13
    {  0.000,  7.274 },  // 14

    // glyph 't'
    {  0.000, -1.000 },  // 0 bounding box min
    { 30.000, 56.000 },  // 1 bounding box max
    { 12.000, 56.000 },  // 2
    { 12.000,  9.300 },  // 3
    { 12.000, -3.000 },  // 4
    { 24.100, -0.250 },  // 5
    { 26.100,  0.180 },  // 6
    { 28.000,  1.250 },  // 7
    {  0.000, 42.000 },  // 8
    { 30.000, 42.000 },  // 9

    // glyph 'u'
    {  0.000, -1.000 },  //  0 bounding box min
    { 34.000, 43.000 },  //  1 bounding box max
    { 34.000,  0.000 },  // 2
    { 34.000, 42.000 },  // 3
    { 34.000, 14.000 },  // 4
    { 30.000,  2.000 },  // 5
    { 19.000, -1.000 },  // 6
    {  0.000, -5.000 },  // 7
    {  0.000, 14.000 },  // 8
    {  0.000, 42.000 },  // 9

    // glyph 'v'
    {  0.000,  0.000 },  // 0 bounding box min
    { 38.000, 42.000 },  // 1 bounding box max
    {  0.000, 42.000 },  // 2
    { 19.000,  0.000 },  // 3
    { 38.000, 42.000 },  // 4

    // glyph 'w'
    {  0.000, 0.0000 },  // 0 bounding box min
    { 54.000, 42.000 },  // 1 bounding box max
    {  0.000, 42.000 },  // 2
    { 13.500,  0.000 },  // 3
    { 27.000, 42.000 },  // 4
    { 40.500,  0.000 },  // 5
    { 54.000, 42.000 },  // 6

    // glyph 'x'
    {  0.000, 0.0000 },  // 0 bounding box min
    { 32.000, 42.000 },  // 1 bounding box max
    {  1.000, 42.000 },  // 2
    { 32.000,  0.000 },  // 3
    { 31.000, 42.000 },  // 4
    {  0.000,  0.000 },  // 5

    // glyph 'y'
    {  0.000,-26.000 },  // 0 bounding box min
    { 38.000, 42.000 },  // 1 bounding box max
    {  0.000, 42.000 },  // 2
    { 19.000,  0.000 },  // 3
    { 38.000, 42.000 },  // 4
    {  7.147000,-26.000 },  // 5

    // glyph 'z'
    {  0.000, 0.0000 },  // 0 bounding box min
    { 32.000, 42.000 },  // 1 bounding box max
    {  1.000, 42.000 },  // 2
    { 31.000, 42.000 },  // 3
    {  0.000,  0.000 },  // 4
    { 32.000,  0.000 },  // 5

    // glyph '{'
    {  0.000,-26.000 },  //  0 bounding box min
    { 14.000, 66.000 },  //  1 bounding box max
    { 14.000, 66.000 },  //  2
    { -2.000, 59.000 },  //  3
    {  7.000, 43.000 },  //  4
    { 16.000, 27.000 },  //  5
    {  0.000, 20.000 },  //  6
    { 16.000, 13.000 },  //  7
    {  7.000, -3.000 },  //  8
    { -2.000,-19.000 },  //  9
    { 14.000,-26.000 },  // 10

    // glyph '|'
    {  0.000,-26.000 },  // 0 bounding box min
    {  4.000, 66.000 },  // 1 bounding box max
    {  2.000,-26.000 },  // 2
    {  2.000, 66.000 },  // 3

    // glyph '}'
    {  0.000,-26.000 },  //  0 bounding box min
    { 14.000, 66.000 },  //  1 bounding box max
    {  0.000, 66.000 },  //  2
    { 16.000, 59.000 },  //  3
    {  7.000, 43.000 },  //  4
    { -2.000, 27.000 },  //  5
    { 14.000, 20.000 },  //  6
    { -2.000, 13.000 },  //  7
    {  7.000, -3.000 },  //  8
    { 16.000,-19.000 },  //  9
    {  0.000,-26.000 },  // 10

    // glyph '~'
    {  0.000, 31.000 },  //  0 bounding box min
    { 38.000, 53.000 },  //  1 bounding box max
    {  0.000, 41.000 },  //  2
    {  4.000, 64.000 },  //  3
    { 19.000, 42.000 },  //  4
    { 34.000, 20.000 },  //  5
    { 38.000, 43.000 },  //  6

    // glyph '\x7f' (triangle symbol for unimplemented chars)
    {  0.000,  0.000 },  // 0 bounding box min
    { 44.000, 60.000 },  // 1 bounding box max
    {  0.000,  0.000 },  // 2
    {  0.000, 22.000 },  // 3
    { 22.000, 42.000 },  // 4
    { 44.000, 22.000 },  // 5
    { 44.000,  0.000 },  // 6
};

//---------------------------------------------------------------------
//
// Each string in the displaylist array element is a display list for
// a glyph. The first byte in each string is the character code for
// the glyph. The second byte is the number of xytbl array elements
// used to construct the glyph. The remaining bytes are interpreted by
// the DrawGlyph function to draw the glyph on the display.
//
//---------------------------------------------------------------------

char *displaylist[] = {

// glyph ' '  (blank space)

    " "  "\x2"         // 2 points for this glyph
    ,                  // end of display list for this glyph

// glyph '!'

    "!"  "\x5"         // 5 points for this glyph
    DRAWADOT "\x2"
    MOVE "\x3"
    LINE "\x4"
    ,                  // end of display list for this glyph

// glyph '\"'

    "\""  "\x6"        // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    ,                  // end of display list for this glyph

// glyph '#'

    "#"  "\xa"         // 10 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    MOVE "\x6"
    LINE "\x7"
    MOVE "\x8"
    LINE "\x9"
    ,                  // end of display list for this glyph

// glyph '$'

    "$"  "\x11"         // 17 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\xc\x3"
    MOVE "\xf"
    LINE "\x10"
    ,                  // end of display list for this glyph

// glyph '%'

    "%"  "\xa"         // 10 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ELLIPSE "\x4\x5\x6"
    ELLIPSE "\x7\x8\x9"
    ,                  // end of display list for this glyph

// glyph '&'

    "&"  "\x17"         // 23 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x14\x3"
    ,                  // end of display list for this glyph

// glyph '\''

    "\'"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph '('

    "("  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x4\x3"
    ,                  // end of display list for this glyph

// glyph ')'

    ")"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x4\x3"
    ,                  // end of display list for this glyph

// glyph '*'

    "*"  "\x8"         // 8 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    MOVE "\x6"
    LINE "\x7"
    ,                  // end of display list for this glyph

// glyph '+'

    "+"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    ,                  // end of display list for this glyph

// glyph ','

    ","  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x2\x4"
    ,                  // end of display list for this glyph

// glyph '-'

    "-"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph '.'

    "."  "\x3"         // 3 points for this glyph
    DRAWADOT "\x2"
    ,                  // end of display list for this glyph

// glyph '/'

    "/"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph '0'

    "0"  "\x5"         // 5 points for this glyph
    ELLIPSE "\x2\x3\x4"
    ,                  // end of display list for this glyph

// glyph '1'

    "1"  "\x5"         // 5 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    ,                  // end of display list for this glyph

// glyph '2'

    "2"  "\xa"         // 10 points for this glyph
    ENDFIGURE
    ELLIPTICARC "\x2\x3\x4\xe0\x40"
    POLYELLIPTICSPLINE "\x4\x5"
    LINE "\x9"
    ,                  // end of display list for this glyph

// glyph '3'

    "3"  "\x11"         // 17 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\xe\x3"
    ,                  // end of display list for this glyph

// glyph '4'

    "4"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    POLYLINE "\x3\x3"
    ,                  // end of display list for this glyph

// glyph '5'

    "5"  "\xd"         // 13 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    POLYELLIPTICSPLINE "\x8\x5"
    ,                  // end of display list for this glyph

// glyph '6'

    "6"  "\xb"         // 11 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    POLYELLIPTICSPLINE "\x2\x6"
    ELLIPTICARC "\x8\x9\xa\x0\x2c"
    ,                  // end of display list for this glyph

// glyph '7'

    "7"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x2\x4"
    ,                  // end of display list for this glyph

// glyph '8'

    "8"  "\x13"         // 19 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x10\x3"
    CLOSEFIGURE
    ,                  // end of display list for this glyph

// glyph '9'

    "9"  "\xb"         // 11 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    POLYELLIPTICSPLINE "\x2\x6"
    ELLIPTICARC "\x8\x9\xa\x0\x2c"
    ,                  // end of display list for this glyph

// glyph ':'

    ":"  "\x4"         // 4 points for this glyph
    DRAWADOT "\x2"
    DRAWADOT "\x3"
    ,                  // end of display list for this glyph

// glyph ';'

    ";"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x2\x4"
    DRAWADOT "\x6"
    ,                  // end of display list for this glyph

// glyph '<'

    "<"  "\x5"         // 5 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    ,                  // end of display list for this glyph

// glyph '='

    "="  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    ,                  // end of display list for this glyph

// glyph '>'

    ">"  "\x5"         // 5 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    ,                  // end of display list for this glyph

// glyph '?'

    "?"  "\x10"         // 16 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\xc\x3"
    DRAWADOT "\xf"
    ,                  // end of display list for this glyph

// glyph '@"

    "@"  "\x12"         // 18 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    POLYELLIPTICSPLINE "\xc\x6"
    ,                  // end of display list for this glyph

// glyph 'A"

    "A"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    ENDFIGURE
    MOVE "\x5"
    LINE "\x6"
    ,                  // end of display list for this glyph

// glyph 'B'

    "B"  "\xf"         // 15 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x4\x4"
    POLYLINE "\x2\x8"
    POLYELLIPTICSPLINE "\x4\xa"
    LINE "\xe"
    CLOSEFIGURE
    ,                  // end of display list for this glyph

// glyph 'C'

    "C"  "\x5"         // 5 points for this glyph
    ENDFIGURE
    ELLIPTICARC "\x2\x3\x4\xf0\x60"
    ,                  // end of display list for this glyph

// glyph 'D'

    "D"  "\x9"         // 9 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x4\x4"
    LINE "\x8"
    CLOSEFIGURE
    ,                  // end of display list for this glyph

// glyph 'E'

    "E"  "\x8"         // 8 points for this glyph
    MOVE "\x2"
    POLYLINE "\x3\x3"
    MOVE "\x6"
    LINE "\x7"
    ,                  // end of display list for this glyph

// glyph 'F'

    "F"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    MOVE "\x5"
    LINE "\x6"
    ,                  // end of display list for this glyph

// glyph 'G'

    "G"  "\x8"         // 8 points for this glyph
    ENDFIGURE
    ELLIPTICARC "\x2\x3\x4\xf0\x60"
    MOVE "\x5"
    POLYLINE "\x2\6"
    ,                  // end of display list for this glyph

// glyph 'H'

    "H"  "\x8"         // 8 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    MOVE "\x6"
    LINE "\x7"
    ,                  // end of display list for this glyph

// glyph 'I'

    "I"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph 'J'

    "J"  "\x8"         // 8 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x4\x3"
    LINE "\x7"
    ,                  // end of display list for this glyph

// glyph 'K'

    "K"  "\x9"         // 9 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x4\x3"
    MOVE "\x7"
    LINE "\x8"
    ,                  // end of display list for this glyph

// glyph 'L'

    "L"  "\x5"         // 5 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    ,                  // end of display list for this glyph

// glyph 'M'

    "M"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYLINE "\x4\x3"
    ,                  // end of display list for this glyph

// glyph 'N'

    "N"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    POLYLINE "\x3\x3"
    ,                  // end of display list for this glyph

// glyph 'O'

    "O"  "\x5"         // 5 points for this glyph
    ELLIPSE "\x2\x3\x4"
    ,                  // end of display list for this glyph

// glyph 'P'

    "P"  "\xa"         // 10 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    POLYELLIPTICSPLINE "\x4\x5"
    LINE "\x9"
    ,                  // end of display list for this glyph

// glyph 'Q'

    "Q"  "\x8"         // 8 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    POLYELLIPTICSPLINE "\x2\x6"
    ,                  // end of display list for this glyph

// glyph 'R'

    "R"  "\xd"         // 13 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    POLYELLIPTICSPLINE "\x4\x5"
    LINE "\x9"
    MOVE "\xa"
    POLYELLIPTICSPLINE "\x2\xb"
    ,                  // end of display list for this glyph

// glyph 'S'

    "S"  "\xf"         // 15 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\xc\x3"
    ,                  // end of display list for this glyph

// glyph 'T'

    "T"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    ,                  // end of display list for this glyph

// glyph 'U'

    "U"  "\x9"         // 9 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x4\x4"
    LINE "\x8"
    ,                  // end of display list for this glyph

// glyph 'V'

    "V"  "\x5"         // 5 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    ,                  // end of display list for this glyph

// glyph 'W'

    "W"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYLINE "\x4\x3"
    ,                  // end of display list for this glyph

// glyph 'X'

    "X"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    ,                  // end of display list for this glyph

// glyph 'Y'

    "Y"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    MOVE "\x5"
    LINE "\x6"
    ,                  // end of display list for this glyph

// glyph 'Z'

    "Z"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    POLYLINE "\x3\x3"
    ,                  // end of display list for this glyph

// glyph '['

    "["  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    POLYLINE "\x3\x3"
    ,                  // end of display list for this glyph

// glyph '\\'

    "\\"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph ']'

    "]"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    POLYLINE "\x3\x3"
    ,                  // end of display list for this glyph

// glyph '^'

    "^"  "\x5"         // 5 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    ,                  // end of display list for this glyph

// glyph '_'

    "_"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph '`'

    "`"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph 'a'

    "a"  "\x7"         // 7 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    LINE "\x6"
    ,                  // end of display list for this glyph

// glyph 'b'

    "b"  "\x7"         // 7 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    LINE "\x6"
    ,                  // end of display list for this glyph

// glyph 'c'

    "c"  "\x5"         // 5 points for this glyph
    ENDFIGURE
    ELLIPTICARC "\x2\x3\x4\xf0\x60"
    ,                  // end of display list for this glyph

// glyph 'd'

    "d"  "\x7"         // 7 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    LINE "\x6"
    ,                  // end of display list for this glyph

// glyph 'e'

    "e"  "\xc"         // 12 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x8\x4"
    ,                  // end of display list for this glyph

// glyph 'f'

    "f"  "\xa"         // 10 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x4\x3"
    LINE "\x7"
    MOVE "\x8"
    LINE "\x9"
    ,                  // end of display list for this glyph

// glyph 'g'

    "g"  "\xb"         // 11 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    LINE "\x6"
    POLYELLIPTICSPLINE "\x4\x7"
    ,                  // end of display list for this glyph

// glyph 'h'

    "h"  "\xa"         // 10 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    POLYELLIPTICSPLINE "\x4\x5"
    LINE "\x9"
    ,                  // end of display list for this glyph

// glyph 'i'

    "i"  "\x5"         // 5 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    DRAWADOT "\x4"
    ,                  // end of display list for this glyph

// glyph 'j'

    "j"  "\x9"         // 9 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x4\x4"
    DRAWADOT "\x8"
    ,                  // end of display list for this glyph

// glyph 'k'

    "k"  "\x8"         // 8 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    MOVE "\x6"
    LINE "\x7"
    ,                  // end of display list for this glyph

// glyph 'l'

    "l"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph 'm'

    "m"  "\x10"         // 16 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    POLYELLIPTICSPLINE "\x4\x5"
    LINE "\x9"
    MOVE "\xa"
    POLYELLIPTICSPLINE "\x4\xb"
    LINE "\xf"
    ,                  // end of display list for this glyph

// glyph 'n'

    "n"  "\xa"         // 10 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    POLYELLIPTICSPLINE "\x4\x5"
    LINE "\x9"
    ,                  // end of display list for this glyph

// glyph 'o'

    "o"  "\x5"         // 5 points for this glyph
    ELLIPSE "\x2\x3\x4"
    ,                  // end of display list for this glyph

// glyph 'p'

    "p"  "\x7"         // 7 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    LINE "\x6"
    ,                  // end of display list for this glyph

// glyph 'q'

    "q"  "\x7"         // 7 points for this glyph
    ELLIPSE "\x2\x3\x4"
    MOVE "\x5"
    LINE "\x6"
    ,                  // end of display list for this glyph

// glyph 'r'

    "r"  "\x9"         // 9 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    POLYELLIPTICSPLINE "\x4\x5"
    ,                  // end of display list for this glyph

// glyph 's'

    "s"  "\xf"         // 15 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\xc\x3"
    ,                  // end of display list for this glyph

// glyph 't'

    "t"  "\xa"         // 10 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    POLYELLIPTICSPLINE "\x4\x4"
    MOVE "\x8"
    LINE "\x9"
    ,                  // end of display list for this glyph

// glyph 'u'

    "u"  "\xa"         // 10 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    POLYELLIPTICSPLINE "\x4\x5"
    LINE "\x9"
    ,                  // end of display list for this glyph

// glyph 'v'

    "v"  "\x5"         // 5 points for this glyph
    MOVE "\x2"
    POLYLINE "\x2\x3"
    ,                  // end of display list for this glyph

// glyph 'w'

    "w"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYLINE "\x4\x3"
    ,                  // end of display list for this glyph

// glyph 'x'

    "x"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    MOVE "\x4"
    LINE "\x5"
    ,                  // end of display list for this glyph

// glyph 'y'

    "y"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    LINE "\x4"
    MOVE "\x3"
    LINE "\x5"
    ,                  // end of display list for this glyph

// glyph 'z'

    "z"  "\x6"         // 6 points for this glyph
    MOVE "\x2"
    POLYLINE "\x3\x3"
    ,                  // end of display list for this glyph

// glyph '{'

    "{"  "\xb"         // 11 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x8\x3"
    ,                  // end of display list for this glyph

// glyph '|'

    "|"  "\x4"         // 4 points for this glyph
    MOVE "\x2"
    LINE "\x3"
    ,                  // end of display list for this glyph

// glyph '}'

    "}"  "\xb"         // 11 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x8\x3"
    ,                  // end of display list for this glyph

// glyph '~'

    "~"  "\x7"         // 7 points for this glyph
    MOVE "\x2"
    POLYELLIPTICSPLINE "\x4\x3"
    ,                  // end of display list for this glyph

// glyph '\x7f' ("house" symbol for unimplemented chars)

    "\x7f"  "\x7"        // 7 points for this glyph
    MOVE "\x2"
    POLYLINE "\x4\x3"
    CLOSEFIGURE
    ,                  // end of display list for this glyph

};

//---------------------------------------------------------------------
//
// Public functions: The TextApp constructor allocates and initializes
// the font data structures
//
//---------------------------------------------------------------------

TextApp::TextApp() : _width(0), _xspace(1.0)
{
    int offset = 0;
    int oldcc = 0;

    memset(_glyphtbl, 0, sizeof(_glyphtbl));
    _glyph = new GLYPH[ARRAY_LEN(displaylist)];
    assert(_glyph != 0);  // out of memory?
    for (int i = 0; i < ARRAY_LEN(displaylist); ++i)
    {
        GLYPH *p = &_glyph[i];
        char *s = displaylist[i];
        int cc = s[0];

        assert(oldcc < cc);
        p->charcode = cc;
        p->xyindex = offset;
        p->xylen = s[1];
        offset += p->xylen;
        p->displist = &s[2];
        p->xy = 0;
        _glyphtbl[cc] = p;
        oldcc = cc;
    }
    assert(offset == ARRAY_LEN(xytbl));
}

TextApp::~TextApp()
{
    for (int i = 0; i < ARRAY_LEN(displaylist); ++i)
    {
        if (_glyph[i].xy != 0)
        {
            delete[] _glyph[i].xy;
        }
    }
    delete[] _glyph;
}

//---------------------------------------------------------------------
//
// Private function: Display list interpreter (draws a glyph)
//
//---------------------------------------------------------------------

void TextApp::DrawGlyph(ShapeGen *sg, char *displist, SGPoint xy[])
{
    signed char *p = reinterpret_cast<signed char*>(displist);

    //sg->BeginPath();
    while (p[0] != 0)
    {
        switch (p[0])
        {
        case _MOVE_:
            sg->Move(xy[p[1]].x, xy[p[1]].y);
            p += 2;
            break;
        case _LINE_:
            sg->Line(xy[p[1]].x, xy[p[1]].y);
            p += 2;
            break;
        case _CLOSEFIGURE_:
            sg->CloseFigure();
            p += 1;
            break;
        case _ENDFIGURE_:
            sg->EndFigure();
            p += 1;
            break;
        case _POLYLINE_:
            sg->PolyLine(&xy[p[2]], p[1]);
            p += 3;
            break;
        case _ELLIPSE_:
            sg->Ellipse(xy[p[1]], xy[p[2]], xy[p[3]]);
            p += 4;
            break;
        case _ELLIPTICARC_:
            sg->EllipticArc(xy[p[1]], xy[p[2]], xy[p[3]],
                            p[4]*(2.0*PI/128.0), p[5]*(2.0*PI/128.0));
            p += 6;
            break;
        case _POLYELLIPTICSPLINE_:
            sg->PolyEllipticSpline(&xy[p[2]], p[1]);
            p += 3;
            break;
        case _DRAWADOT_:
            {
                SGPoint v0, v1, v2;
                SGCoord radius = 65536*(_width/11 + 0.5f);

                v0 = v1 = v2 = xy[p[1]];
                v1.x += radius;
                v2.y += radius;
                sg->Ellipse(v0, v1, v2);
            }
            p += 2;
            break;
        default:
            assert(0);
            break;
        }
    }
    //sg->StrokePath();
}

//---------------------------------------------------------------------
//
// Public function: Draws the glyphs for the specified character
// string (pointed to by parameter str). Parameter sg is a pointer to
// a ShapeGen object. Parameter xform is the 6-element affine trans-
// formation matrix to apply to the glyphs before they are displayed,
// and is defined as in the SVG standard. Elements xform[4] and
// xform[5] specify the x and y coordinates at which to start drawing
// the string on the display; the starting point is located at the
// intersection of the baseline with the left edge of the displayed
// string. The other four xform elements specify the scaling,
// rotation, etc., to apply to the glyphs. Glyphs are drawn with the
// current stroke width.
//
//---------------------------------------------------------------------

void TextApp::DisplayText(ShapeGen *sg, const float xform[], const char *str)
{
    const int MAXLEN = 256;
    int nbits = sg->SetFixedBits(16);
    LINEEND saveLineEnd = sg->SetLineEnd(LINEEND_ROUND);
    LINEJOIN saveLineJoin = sg->SetLineJoin(LINEJOIN_ROUND);
    int len = strnlen(str, MAXLEN);
    float lbear = _xspace*leftbearing;
    float rbear = _xspace*rightbearing;
    float advance;
    XY pos;

    _width = sg->SetLineWidth(0);
    sg->SetLineWidth(_width);
    assert(len < MAXLEN);
    pos.x = xform[0]*lbear + xform[4];
    pos.y = xform[1]*lbear + xform[5];

    // Each iteration of this for-loop draws one character
    sg->BeginPath();
    for (int i = 0; i < len; ++i)
    {
        int cc = str[i];
        GLYPH *p = _glyphtbl[cc & 0x7f];

        // If glyph is not implemented, use default symbol
        if (cc > 0x7f || p == 0)
            p = _glyphtbl[0x7f];

        // Point to start of xytbl entries for this glyph
        const XY *pxytbl = &xytbl[p->xyindex];

        // Transform the x-y coordinates for this glyph, and convert
        // them to 16.16 fixed-point format
        if (p->xy == 0 && p->xylen != 0)
        {
            // Allocate buffer for transformed x-y coordinates
            p->xy = new SGPoint[p->xylen];
            assert(p->xy != 0);
        }
        for (int j = 2; j < p->xylen; ++j)
        {
            float xtbl = pxytbl[j].x;
            float ytbl = pxytbl[j].y;

            p->xy[j].x = 65536*(xform[0]*xtbl - xform[2]*ytbl + pos.x);
            p->xy[j].y = 65536*(xform[1]*xtbl - xform[3]*ytbl + pos.y);
        }

        // Interpret display list for this glyph
        DrawGlyph(sg, p->displist, p->xy);

        // Advance to x-y position of next glyph
        advance = lbear + (pxytbl[1].x - pxytbl[0].x) + rbear;
        pos.x += xform[0]*advance;
        pos.y += xform[1]*advance;
    }
    sg->StrokePath();
    sg->SetFixedBits(nbits);  // restore caller's original settings
    sg->SetLineEnd(saveLineEnd);
    sg->SetLineJoin(saveLineJoin);
}

//---------------------------------------------------------------------
//
// Public function: Draws the glyphs for the specified text string.
// This version of the function draws only horizontal text, but
// supports translation (to starting point xystart) and scaling (by
// the scale parameter value). Glyphs are drawn with the current
// stroke width.
//
//---------------------------------------------------------------------

void TextApp::DisplayText(ShapeGen *sg, SGPoint xystart, float scale, const char *str)
{
    float xform[6];

    xform[0] = scale;
    xform[1] = 0;
    xform[2] = 0;
    xform[3] = scale;
    xform[4] = xystart.x;
    xform[5] = xystart.y;
    DisplayText(sg, xform, str);
}

//---------------------------------------------------------------------
//
// Public function: Calculates what the x-y coordinates would be at
// the end of a text string if the string were to be displayed with
// the specified transform and current text spacing factor. (Note that
// nothing is actually drawn by this function.) Parameter xform is the
// 6-element affine transformation matrix, and is defined as in the
// SVG standard. Parameter str is the text string. Parameter xyout is
// the output pointer for the end-point coordinates. The end point
// lies at the intersection of the transformed glyph baseline with the
// right edge of the transformed string.
//
//---------------------------------------------------------------------

void TextApp::GetTextEndpoint(const float xform[], const char *str, XY *xyout)
{
    const int MAXLEN = 256;
    int len = strnlen(str, MAXLEN);
    float advance = 0;

    assert(xyout != 0);
    assert(len < MAXLEN);

    // Each iteration of this for-loop adds the width of a glyph
    for (int i = 0; i < len; ++i)
    {
        int cc = str[i];
        GLYPH *p = _glyphtbl[cc & 0x7f];

        // If glyph is not implemented, use default symbol
        if (cc > 0x7f || p == 0)
            p = _glyphtbl[0x7f];

        // Point to the start of the xytbl entries for this glyph
        const XY *pxytbl = &xytbl[p->xyindex];

        // First two entries are bounding box min/max coordinates
        advance += pxytbl[1].x - pxytbl[0].x;
    }

    // Advance to x-y position at end of last glyph
    advance += _xspace*len*(leftbearing + rightbearing);
    xyout->x = xform[0]*advance + xform[4];
    xyout->y = xform[1]*advance + xform[5];
}

//---------------------------------------------------------------------
//
// Public function: Returns the width in pixels of a horizontal text
// string drawn to the specified scale and with the current text-
// spacing factor
//
//---------------------------------------------------------------------

float TextApp::GetTextWidth(float scale, const char *str)
{
    XY xyout;
    float xform[6];
    SGPoint xystart = { 0, 0 };

    xform[0] = scale;
    xform[1] = 0;
    xform[2] = 0;
    xform[3] = scale;
    xform[4] = xystart.x;
    xform[5] = xystart.y;
    GetTextEndpoint(xform, str, &xyout);
    return xyout.x;
}

//---------------------------------------------------------------------
//
// Public function: Sets the text-spacing factor. Parameter xspace is
// the character-spacing multiplier. To expand the spacing between
// characters, specify an xspace value greater than 1.0. For example,
// an xspace value of 1.2 specifies a 20 percent increase over the
// default spacing. To decrease the spacing between characters,
// specify an xspace value less than one. An xspace value of 1.0
// restores the default character spacing.
//
//---------------------------------------------------------------------

void TextApp::SetTextSpacing(float xspace)
{
    _xspace = xspace;
}

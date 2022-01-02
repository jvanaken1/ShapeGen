/*
  Copyright (C) 2019-2022 Jerry R. VanAken

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
// shapegen.h:
//   Public header file for the ShapeGen interface
//
//---------------------------------------------------------------------

#ifndef SHAPEGEN_H
  #define SHAPEGEN_H

//---------------------------------------------------------------------
//
// Define types and constants
//
//---------------------------------------------------------------------

// 32-bit fixed-point value with 16-bit fraction (16.16 fixed point) 
typedef int FIX16;

// Coordinates, points, rectangles, and spans
typedef int SGCoord;
struct SGPoint { 
    SGCoord x; 
    SGCoord y; 
};
struct SGRect { 
    SGCoord x; 
    SGCoord y; 
    SGCoord w; 
    SGCoord h; 
};
struct SGSpan {  // <-- used only by a renderer 
    FIX16 xL; 
    FIX16 xR;
    int y; 
};

// Default line-width attribute for stroked paths 
const float LINEWIDTH_DEFAULT = 4.0;

// Miter limit parameters 
const float MITERLIMIT_DEFAULT = 10.0;   // default miter limit
const float MITERLIMIT_MINIMUM = 1.0;    // minimum miter limit

// Flatness threshold -- required curve-to-chord tolerance 
const float FLATNESS_DEFAULT = 0.5;      // default flatness setting
const float FLATNESS_MINIMUM = 0.2;      // minimum flatness setting
const float FLATNESS_MAXIMUM = 100.0;    // maximum flatness setting

// SGCoord fixed-point fraction length -- bits to right of binary point 
const int FIXBITS_DEFAULT = 0;  // default = integer (no fixed point)

// Maximum length of dash-pattern array, not counting terminating 0
const int DASHARRAY_MAXLEN = 32;

// Fill rule attributes for filling paths 
enum FILLRULE {
    FILLRULE_EVENODD,    // even-odd (aka "parity") fill rule
    FILLRULE_WINDING,    // nonzero winding number fill rule
};

// Join attribute values for stroked paths 
enum LINEJOIN {
    LINEJOIN_BEVEL,  // beveled join between two line segments
    LINEJOIN_ROUND,  // rounded join between two line segments
    LINEJOIN_MITER   // mitered join between two line segments
};
const LINEJOIN LINEJOIN_DEFAULT = LINEJOIN_BEVEL;  // default line join

// Line end cap attribute values for stroked paths 
enum LINEEND {
    LINEEND_FLAT,   // flat line end (butt line cap)
    LINEEND_ROUND,  // rounded line end (round cap)
    LINEEND_SQUARE  // squared line end (projecting cap)
};
const LINEEND LINEEND_DEFAULT = LINEEND_FLAT;  // default line end

//---------------------------------------------------------------------
//
// Shape feeder: Breaks a shape into smaller pieces to feed to a
// renderer. The ShapeGen object loads a shape into a shape feeder and
// then hands off the feeder to the renderer. The renderer iteratively
// calls one of the three functions below to receive the shape in
// pieces -- as rectangles or spans -- that are ready to be drawn to
// the graphics display as filled shapes. The GetNextSDLRect function
// dispenses rectangles in SDL2 (Simple DirectMedia Layer, version 2)
// SDL_Rect format. The GetNextGDIRect function dispenses rectangles
// in Windows GDI's RECT format (in spite of the somewhat misleading
// type cast to SGRect*). The GetNextSGSpan function supports
// antialiasing by dispensing a subpixel span that adds a horizontal
// row of bits to the coverage bitmaps for a row of pixels.
//
//---------------------------------------------------------------------

class ShapeFeeder
{
public:
    virtual bool GetNextSDLRect(SGRect *rect) = 0;
    virtual bool GetNextGDIRect(SGRect *rect) = 0;
    virtual bool GetNextSGSpan(SGSpan *span) = 0;
};

//---------------------------------------------------------------------
//
// Renderer: Handles requests from the ShapeGen object to draw filled
// shapes on the display. An antialiasing renderer implements its own
// versions of the QueryYResolution and SetMaxWidth functions, but a
// simple renderer simply inherits the rudimentary versions defined
// here. To enable pattern alignment, an enhanced renderer implements
// its own version of the SetScrollPosition function, but a renderer
// that does only solid-color fills can inherit the version below.
//
//---------------------------------------------------------------------

class Renderer
{
public:
    virtual void RenderShape(ShapeFeeder *feeder) = 0;
    virtual int QueryYResolution() { return 0; }
    virtual bool SetMaxWidth(int width) { return true; }
    virtual bool SetScrollPosition(int x, int y) { return true; }
};

//---------------------------------------------------------------------
//
// ShapeGen class: 2-D Polygonal Shape Generator. Constructs paths
// that describe polygonal shapes, and then maps these shapes onto the
// pixel grid. But the ShapeGen object relies on the Renderer object
// to actually write the shapes to the pixels in the display device.
// In other words, ShapeGen consigns all device dependencies to the
// renderer. Thus, the ShapeGen source code contains no device depend-
// encies or platform-specific function calls, and is highly portable. 
//
//---------------------------------------------------------------------

class ShapeGen
{
public:
    ShapeGen()
    {
    }
    virtual ~ShapeGen()
    {
    }

    // Renderer object
    virtual void SetRenderer(Renderer *renderer) = 0;

    // Basic path construction
    virtual void BeginPath() = 0;
    virtual void CloseFigure() = 0;
    virtual void EndFigure() = 0;
    virtual void Move(SGCoord x, SGCoord y) = 0;
    virtual bool Line(SGCoord x, SGCoord y) = 0;
    virtual bool PolyLine(int npts, const SGPoint xy[]) = 0;
    virtual void Rectangle(const SGRect& rect) = 0;
    virtual bool FillPath(FILLRULE fillrule) = 0;
    virtual void SetScrollPosition(int x, int y) = 0;
    virtual bool GetCurrentPoint(SGPoint *cpoint) = 0;
    virtual bool GetFirstPoint(SGPoint *fpoint) = 0;
    virtual int GetBoundingBox(SGRect *bbox) = 0;

    // Clipping and masking
    virtual void InitClipRegion(int width, int height) = 0;
    virtual void ResetClipRegion() = 0;
    virtual bool SetClipRegion(FILLRULE fillrule) = 0;
    virtual bool SetMaskRegion(FILLRULE fillrule) = 0;
    virtual bool SaveClipRegion() = 0;
    virtual bool SwapClipRegion() = 0;

    // Basic path attributes
    virtual float SetFlatness(float tol) = 0;
    virtual int SetFixedBits(int nbits) = 0;

    // Stroked path construction
    virtual bool StrokePath() = 0;

    // Stroked path attributes
    virtual float SetLineWidth(float width) = 0;
    virtual float SetMiterLimit(float mlim) = 0;
    virtual LINEEND SetLineEnd(LINEEND capstyle) = 0;
    virtual LINEJOIN SetLineJoin(LINEJOIN joinstyle) = 0;
    virtual void SetLineDash(char *dash, int offset, float mult) = 0;

    // Ellipses and elliptic arcs
    virtual void Ellipse(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2) = 0;
    virtual void EllipticArc(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2, 
                             float aStart, float aSweep) = 0;
    virtual bool EllipticSpline(const SGPoint& v1, const SGPoint& v2) = 0;
    virtual bool PolyEllipticSpline(int npts, const SGPoint xy[]) = 0;
    virtual void RoundedRectangle(const SGRect& rect, const SGPoint& round) = 0;

    // Bezier splines (quadratic and cubic)
    virtual bool Bezier2(const SGPoint& v1, const SGPoint& v2) = 0;
    virtual bool PolyBezier2(int npts, const SGPoint xy[]) = 0;
    virtual bool Bezier3(const SGPoint& v1, const SGPoint& v2, const SGPoint& v3) = 0;
    virtual bool PolyBezier3(int npts, const SGPoint xy[]) = 0;
};

//---------------------------------------------------------------------
//
// SGPtr class: ShapeGen smart pointer. Creates a ShapeGen object that
// is automatically deleted when the SGPtr object goes out of scope.
// The SGPtr class encapsulates ShapeGen's internal implementation
// details as follows: (1) SGPtr overloads the '->' operator to enable
// an SGPtr object to be used as a ShapeGen object pointer, and (2)
// SGPtr overloads the '*' operator to enable a ShapeGen object
// pointer to be passed as a function parameter.
//
//--------------------------------------------------------------------- 
class SGPtr 
{ 
    ShapeGen *sg;

    ShapeGen* CreateShapeGen(Renderer *renderer, const SGRect& cliprect);

public: 
    SGPtr(Renderer *renderer, const SGRect& cliprect) 
    { 
        sg = CreateShapeGen(renderer, cliprect); 
    }
    ~SGPtr() 
    { 
        delete(sg); 
    }
    ShapeGen* operator->() 
    { 
        return sg; 
    }
    ShapeGen& operator*() 
    {  
        return *sg; 
    } 
};

#endif  // SHAPEGEN_H

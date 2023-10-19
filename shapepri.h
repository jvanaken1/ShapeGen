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
// shapepri.h:
//   Private header file for the internal implementation of the
//   ShapeGen class. Defines constants, structures, utility functions,
//   and helper classes.
//
//---------------------------------------------------------------------

#ifndef SHAPEPRI_H
    #define SHAPEPRI_H

#include <math.h>
#include <string.h>
#include <assert.h>
#include "shapegen.h"

// Macro definitions
#define ARRAY_LEN(a)  (sizeof(a)/sizeof((a)[0]))
#ifdef min
  #undef min
#endif
#define min(x,y)  ((x)<(y)?(x):(y))  // take minimum of two values
#ifdef max
  #undef max
#endif
#define max(x,y)  ((x)>(y)?(x):(y))  // take maximum of two values

//---------------------------------------------------------------------
//
// Constants and structures
//
//---------------------------------------------------------------------

// Internal structures for 2-D points, vertices, and vectors
struct VERT16 {
    FIX16 x, y;
};
struct XY {
    float x;
    float y;
};

const int BIGVAL16 = 0x7FFF;  // biggest 16-bit signed integer value

// Miscellaneous constants in 16.16 internal fixed-point format
const FIX16 FIX_PI   = 205887;  // pi radians in fixed-point format
const FIX16 FIX_2PI  = 411775;  // 2*pi radians in fixed-point format
const FIX16 FIX_HALF = 0x8000;  // (1/2) in fixed-point format
const FIX16 FIX_BIAS = 0x7FFF;  // (1/2-1/65536) in fixed-point format

// Constants used to generate quadratic and cubic spline curves
const int KMAX = 6;        // max k for ellipse angular increment 1/2^k
const int MAXLEVELS = 12;  // max number bezier subdivision levels

// Structure used to describe a figure (aka subpath or contour)
struct FIGURE {
    bool isclosed;  // true if figure is closed
    int offset;     // offset to previous item in list of figures
};

// Structure used to describe a polygonal edge
struct EDGE {
    EDGE *next;  // pointer to next item in edge list
    int ytop;    // integer y coord at top end of edge
    int dy;      // signed length of edge in y dimension
    FIX16 xtop;  // fixed point x coord at top end of edge
    FIX16 dxdy;  // inverse slope: change in x per unit step in y
};

// Singly linked list of edges
struct EDGELIST {
    EDGE *head;
    EDGE *tail;
    EDGELIST() : head(0), tail(0) {}
    ~EDGELIST() {}
};

// Internal fill-rule attribute values to set clipping region:
//   FILLRULE_INTERSECT - Include spans that are inside both the
//       current clipping region and the current path
//   FILLRULE_EXCLUDE - Include spans that are inside the current
//       clipping region but outside the current path
const FILLRULE FILLRULE_INTERSECT = FILLRULE(FILLRULE_WINDING + 1);
const FILLRULE FILLRULE_EXCLUDE = FILLRULE(FILLRULE_INTERSECT + 1);

// Length of initial path stack memory allocation
const int INITIAL_PATH_LENGTH = 2000;

// Default length of initial POOL memory allocation
const int INITIAL_POOL_LENGTH = 2000;

// Fixed length of POOL inventory of fully allocated blocks
const int POOL_INVENTORY_LENGTH = 20;

//---------------------------------------------------------------------
//
// Storage pool of EDGE structures that we assume are allocated one at
// a time, and which are all later freed simultaneously
//
//---------------------------------------------------------------------

class POOL
{
    EDGE *block;    // block currently in use for EDGE allocations
    int blklen;     // number of EDGE structures in block array
    int watermark;  // allocation watermark in current block

    // Track inventory of exhausted (fully allocated) blocks
    EDGE *inventory[POOL_INVENTORY_LENGTH];
    int count;  // number of EDGE structures in inventory array
    int index;  // index of next free slot in inventory array

    void AcquireBlock();  // add new block of memory to pool

public:
    POOL(int len = INITIAL_POOL_LENGTH);
    ~POOL();
    void Reset();
    EDGE* Allocate(EDGE *p = 0);
    int GetCount()
    {
        return (count + watermark);
    }
};

//---------------------------------------------------------------------
//
// Edge manager. Converts a path to a set of polygonal edges, clips
// these edges, and produces a set of non-overlapping trapezoids that
// are ready to be rendered and displayed.
//
//---------------------------------------------------------------------

class PathMgr;  // forward declaration

class EdgeMgr
{
    friend PathMgr;

    EDGELIST _inlist, _outlist, _cliplist, _rendlist, _savelist;
    POOL *_inpool, *_outpool, *_clippool, *_rendpool, *_savepool;
    Renderer *_renderer;
    int _yshift, _ybias, _yhalf;

    void SaveEdgePair(int height, EDGE *edgeL, EDGE *edgeR);

protected:
    EdgeMgr();
    ~EdgeMgr();
    bool SetRenderer(Renderer *renderer);
    bool SetClipList();
    void ReverseEdges();
    void ClipEdges(FILLRULE fillrule);
    bool FillEdgeList();
    void NormalizeEdges(FILLRULE fillrule);
    void AttachEdge(const VERT16 *v1, const VERT16 *v2);
    void TranslateEdges(int x, int y);
    void SetDeviceClipRectangle(int width, int height, bool bsave);
    bool SaveClipRegion();
    bool SwapClipRegion();
};

//---------------------------------------------------------------------
//
// Path Manager. Constructs a path consisting of one or more figures
// (aka subpaths or contours). Each figure is a set of connected points
// that can be either closed (i.e., the first point connects to the
// last point) or open (the first and last points are not connected).
//
//---------------------------------------------------------------------

class PathMgr : virtual public ShapeGen
{
    friend ShapeGen* CreateShapeGen(Renderer*, const SGRect&);

    Renderer *_renderer; // renders filled and stroked shapes
    EdgeMgr *_edge;      // manages lists of polygonal edges
    SGRect _devicecliprect;  // clipping rectangle for display device
    FIX16 _flatness;     // error tolerance for flattened arcs/curves
    int _fixshift;       // to convert user coords to 16.16 fixed-point
    FILLRULE _fillrule;  // either even-odd or nonzero winding number

    // Storage for points in path
    int _pathlength;  // current length of path array
    VERT16 *_path;    // pointer to dynamically allocated path array

    // Path memory management functions
    void GrowPath();
    void PathCheck(VERT16 *ptr)
    {
        if (ptr == &_path[_pathlength])  // detect path overflow
            GrowPath();
    }

    // Four member variables updated by path memory management
    VERT16 *_cpoint;    // pointer to current point in figure
    VERT16 *_fpoint;    // pointer to first point in figure
    FIGURE *_figure;    // pointer to current figure in path
    FIGURE *_figtmp;    // temporary figure pointer

    // Dashed line pattern parameters
    FIX16 _dasharray[DASHARRAY_MAXLEN+1];  // dash pattern storage
    FIX16 _dashoffset;  // starting offset into dashed-line pattern
    FIX16 *_pdash;      // pointer to current position in dash array
    FIX16 _dashlen;     // length in pixels of the current dash/gap
    bool _dashon;       // true (false) if _pdash points to dash (gap)

    // Stroked path variables
    VERT16 _vin, _vout; // edge vertices at start of stroked segment
    FIX16 _linewidth;   // width (in pixels) of stroked line
    LINEEND _lineend;   // line end cap style
    LINEJOIN _linejoin; // line join style
    float _miterlimit;  // miter limit
    float _mitercheck;  // precomputed parameter for miter-limit check
    FIX16 _angle;       // angle between line segments at round join

    void FinalizeFigure(bool bclose);  // closes or ends a figure

protected:
    PathMgr(Renderer *renderer, const SGRect& cliprect);
    ~PathMgr();

public:
    // Selects the renderer to use for filling and stroking shapes
    bool SetRenderer(Renderer *renderer);

    // Basic path construction
    void BeginPath();
    void CloseFigure();
    void EndFigure();
    void Move(SGCoord x, SGCoord y);
    bool Line(SGCoord x, SGCoord y);
    bool PolyLine(const SGPoint xy[], int npts);
    void Rectangle(const SGRect& rect);

    // Basic path attributes
    float SetFlatness(float tol);
    int SetFixedBits(int nbits);
    void SetScrollPosition(int x, int y);
    bool GetCurrentPoint(SGPoint *cpoint);
    bool GetFirstPoint(SGPoint *fpoint);
    int GetBoundingBox(SGRect *bbox, int flags);

    // Clipping and masking
    bool InitClipRegion(int width, int height);
    void ResetClipRegion();
    bool SetClipRegion(CLIPMODE clipmode);
    bool SetMaskRegion(CLIPMODE clipmode);
    bool SaveClipRegion()
    {
        return _edge->SaveClipRegion();
    }
    bool SwapClipRegion()
    {
        return _edge->SwapClipRegion();
    }

    // Rendering of filled and stroked shapes
    bool FillPath();
    bool StrokePath();

    // Attributes for filling and stroking paths
    FILLRULE SetFillRule(FILLRULE fillrule);
    float SetLineWidth(float width);
    float SetMiterLimit(float mlim);
    LINEEND SetLineEnd(LINEEND capstyle);
    LINEJOIN SetLineJoin(LINEJOIN joinstyle);
    bool SetLineDash(const char dash[], int offset, float mult);

private:
    // Internal functions for filled and stroked paths
    bool FilledShape();  // convert path to edge list for filled shape
    bool StrokedShape(); // convert path to edge list for stroked shape
    bool InitLineDash();
    FIX16 LineLength(const VERT16& vs, const VERT16& ve, XY *u, VERT16 *a);
    void RoundJoin(const VERT16& v0, const VERT16& a1, const VERT16& a2);
    void JoinLines(const VERT16& v0, const VERT16& ain, const VERT16& aout);
    void DashedLine(const VERT16& ve, const XY& u, const VERT16& a, FIX16 linelen);
    bool ThinStroke();
    void JoinThinLines(const VERT16 *vert, int inquad, int outquad);

public:
    // Ellipses and elliptic arcs
    void Ellipse(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2);
    void EllipticArc(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2, float aStart, float aSweep);
    bool EllipticSpline(const SGPoint& v1, const SGPoint& v2);
    bool PolyEllipticSpline(const SGPoint xy[], int npts);
    void RoundedRectangle(const SGRect& rect, const SGPoint& round);

private:
    // Internal functions to flatten ellipses and elliptic arcs
    void EllipseCore(FIX16 xC, FIX16 yC, FIX16 xP, FIX16 yP,
                     FIX16 xQ, FIX16 yQ, FIX16 sweep);
    int AngularInc(FIX16 xP, FIX16 yP, FIX16 xQ, FIX16 yQ);

public:
    // Bezier splines (quadratic and cubic)
    bool Bezier2(const SGPoint& v1, const SGPoint& v2);
    bool PolyBezier2(const SGPoint xy[], int npts);
    bool Bezier3(const SGPoint& v1, const SGPoint& v2, const SGPoint& v3);
    bool PolyBezier3(const SGPoint xy[], int npts);

private:
    // Internal functions for checking flatness of splines
    bool IsFlatQuadratic(const VERT16 v[3]);
    bool IsFlatCubic(const VERT16 v[4]);
};

#endif SHAPEPRI_H

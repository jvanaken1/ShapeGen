/*
  Copyright (C) 2019 Jerry R. VanAken

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
#ifdef sign
#undef sign
#endif 
#define sign(x)   ((x)<0?-1:1)	     // sign (plus or minus) of value

//---------------------------------------------------------------------
//
// Constants and structures
//
//---------------------------------------------------------------------

// Types used internally for fixed-point numbers 
//typedef int FIX16;	 // fixed-point value with 16-bit fraction 
typedef int FIX30;	 // fixed-point value with 30-bit fraction

// Internal structures for 2-D points, vertexes, and vectors
struct VERT16 { FIX16 x, y; };
struct VERT30 { FIX30 x, y; };

const int BIGVAL16 = 0x7FFF;  // biggest 16-bit signed integer value

// Miscellaneous constants in 16.16 internal fixed-point format
const FIX16 FIX_PI   = 205887;  // pi radians in fixed-point format
const FIX16 FIX_HALF = 0x8000;  // (1/2) in fixed-point format
const FIX16 FIX_BIAS = 0x7FFF;  // (1/2-1/65536) in fixed-point format

// Constants used to generate quadratic and cubic spline curves
const int MAXRHOS = 5;     // max. no. of non-unity sharpnesses
const int MAXLEVELS = 12;  // max. no. of subdivision levels

// Structure used to describe a figure (aka subpath or contour) 
struct FIGURE { 
    bool isclosed;  // true if figure is closed
    int offset;     // offset to previous item in list of figures 
};

// Structure used to describe a polygonal edge 
struct EDGE { 
    EDGE *next;  // pointer to next item in edge list
    short ytop;  // integer y coord at top end of edge 
    short dy;	 // signed length of edge in y dimension 
    FIX16 xtop;  // fixed point x coord at top end of edge 
    FIX16 dxdy;  // inverse slope: change in x per unit step in y 
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
    int watermark;  // allocation watermark in block array

    // Track of inventory of exhausted (fully allocated) blocks
    EDGE *inventory[POOL_INVENTORY_LENGTH];
    int count;  // number of EDGE structures in inventory array
    int index;  // index of next free slot in inventory array

    void AcquireBlock();

public:
    POOL(int len = INITIAL_POOL_LENGTH);
    ~POOL();
    void Reset();
    EDGE* Allocate()
    {
        if (watermark == blklen)  // is this block exhausted?
            AcquireBlock();  // yes, acquire more pool memory
        
        return &block[watermark++];
    }
    int GetCount()
    {
        return (count + watermark);
    }
};

//---------------------------------------------------------------------
//
// Edge manager. Converts a path to a set of polygonal edges, clips
// these edges, and produces a set of non-overlapping trapezoids that
// are ready to be rendered on the display.
//
//---------------------------------------------------------------------

class PathMgr;  // forward declaration

class EdgeMgr
{
    friend PathMgr;

    EDGE *_inlist, *_outlist, *_cliplist, *_rendlist, *_savelist;
    POOL *_inpool, *_outpool, *_clippool, *_rendpool, *_savepool;

    void SaveEdgePair(int height, EDGE *edgeL, EDGE *edgeR);

protected:
    Renderer *_renderer;
    EdgeMgr(Renderer *renderer);
    ~EdgeMgr();
    bool SetClipList();
    void ReverseEdges();
    void ClipEdges(FILLRULE fillrule);
    bool FillEdgeList();
    void NormalizeEdges(FILLRULE fillrule);
    void AttachEdge(const VERT16 *v1, const VERT16 *v2);
    void TranslateEdges(int x, int y);
    void SetDeviceClipRectangle(const SGRect *rect);
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
    friend SGPtr;

    EdgeMgr *_edge;     // manages lists of polygonal edges
    SGPoint _scroll;    // integer x-y window scrolling position
    SGRect _devicecliprect;  // clipping rectangle for display device
    FIX16 _flatness;    // error tolerance for flattened arcs/curves
    int _fixshift;      // to convert user coords to 16.16 fixed-point

    // Storage for points in path
    int _pathlength;  // current length of path array
    VERT16 *_path;    // pointer to dynamically allocated path array 

    // Four members updated by path memory management               
    VERT16 *_cpoint;  // pointer to current point in figure              
    VERT16 *_fpoint;  // pointer to first point in figure
    FIGURE *_figure;  // pointer to current figure in path              
    FIGURE *_figtmp;  // temporary figure pointer

    // Dashed line pattern parameters
    FIX16 _dasharray[16];  // zero-terminated dashed-line pattern array
    FIX16 _dashoffset;     // starting offset into dashed-line pattern
    FIX16 *_pdash;         // pointer to current position in dash array
    FIX16 _dashlen;        // length in pixels of the current dash/gap
    bool _dashon;          // true (false) if _pdash points to dash (gap)

    // Stroked path variables
    VERT16 _vin, _vout; // line join in/out vectors of length linewidth/2
    FIX16 _linewidth;   // width of stroked line        
    LINEEND _lineend;   // line end (aka cap) style      
    LINEJOIN _linejoin; // line join style
    FIX16 _miterlimit;  // miter limit              
    FIX16 _mitercheck;  // precomputed parameter for miter-limit check
    
    bool PathToEdges();   // convert a path to an edge list
    void FinalizeFigure(bool bclose);  // close or end a figure

    // Path memory management functions
    void ExpandPath();
    void PathCheck(VERT16 *ptr)
    {
        if (ptr == &_path[_pathlength])
            ExpandPath();
    }

protected:
    PathMgr(Renderer *renderer, const SGRect& cliprect);
    ~PathMgr();

public:

    // Set rendering object
    void SetRenderer(Renderer *renderer);

    // Basic path construction
    void BeginPath();
    void CloseFigure();
    void EndFigure();
    void Move(SGCoord x, SGCoord y);              
    bool Line(SGCoord x, SGCoord y);
    bool PolyLine(int npts, const SGPoint xy[]);
    void Rectangle(const SGRect& rect);
    bool FillPath(FILLRULE fillrule);
    void SetScrollPosition(int x, int y);
    bool GetCurrentPoint(SGPoint *cpoint);
    bool GetFirstPoint(SGPoint *fpoint);
    bool GetBoundingBox(SGRect *bbox);

    // Clipping and masking
    void InitClipRegion(int width, int height); 
    void ResetClipRegion();
    bool SetClipRegion(FILLRULE fillrule);
    bool SetMaskRegion(FILLRULE fillrule);
    bool SaveClipRegion()
    {
        return _edge->SaveClipRegion();
    }
    bool SwapClipRegion()
    {
        return _edge->SwapClipRegion();
    }

    // Basic path attributes
    float SetFlatness(float tol);
    int SetFixedBits(int nbits);

    // Stroked path construction
    bool StrokePath();
    
    // Stroked path attributes
    float SetLineWidth(float width);
    float SetMiterLimit(float mlim);
    LINEEND SetLineEnd(LINEEND capstyle);
    LINEJOIN SetLineJoin(LINEJOIN joinstyle);
    void SetLineDash(char *dash, int offset, float mult);

private:
    // Stroked path internal functions
    bool InitLineDash();
    FIX16 LineLength(const VERT16& vs, const VERT16& ve, VERT30 *u, VERT16 *a);
    void RoundCap(const VERT16& vert, VERT16 a, int asign);
    void JoinLines(const VERT16& v0, const VERT16& ain, const VERT16& aout);
    void DashedLine(const VERT16& ve, const VERT30& u, const VERT16& a, FIX16 linelen);
    bool ThinStrokePath();
    void ThinJoinLines(const VERT16 *vert, int inquad, int outquad);
    
public:
    // Ellipses and elliptic arcs
    void Ellipse(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2);
    void EllipticArc(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2, float aStart, float aSweep);
    bool EllipticSpline(const SGPoint& v1, const SGPoint& v2);
    bool PolyEllipticSpline(int npts, const SGPoint xy[]);
    void RoundedRectangle(const SGRect& rect, const SGPoint& round);

private:
    // Internal function to flatten ellipses and elliptic arcs
    void FlattenArc(const VERT16& v0, const VERT16& v1, VERT16 v2, VERT16 v3, FIX16 angle);
    
public:
    // Bezier splines (quadratic and cubic)
    bool Bezier2(const SGPoint& v1, const SGPoint& v2);
    bool PolyBezier2(int npts, const SGPoint xy[]);
    bool Bezier3(const SGPoint& v1, const SGPoint& v2, const SGPoint& v3);
    bool PolyBezier3(int npts, const SGPoint xy[]);

private:
    // Internal functions for for checking flatness of splines
    bool IsFlatQuadratic(const VERT16 v[]);
    bool IsFlatCubic(const VERT16 v[]);
};

//-----------------------------------------------------------------------
//
// Fixed-point multiply function: fixmpy(x, y, n)
//   Takes two 32-bit, fixed-point values, x and y, both with n bits of
//   fraction, and multiplies them together. Returns a 32-bit product in
//   the same fixed-point format.
//
// Fixed-point divide function: fixdiv(y, x, n)
//   Takes 32-bit, fixed-point dividend x, with n bits of fraction, and
//   divides it by 32-bit divisor y, which is in the same fixed-point
//   format. Returns a 32-bit quotient in the same fixed-point format.
//
// Fixed-point vector magnitude function: fixlen(x, y)
//   Takes a 2-D vector represented by its x-y components, and
//   calculates the length of the vector. Components x and y are
//   represented as 32-bit, fixed-point numbers. Returns the length
//   in the same fixed-point format.
//
//-----------------------------------------------------------------------

inline FIX16 fixmpy(FIX16 x, FIX16 y, int n)
{
    double a = x;
    double b = y;
    double c = 1 << n;
    FIX16 z = (a*b)/c;

    return z;
}

inline FIX16 fixdiv(FIX16 x, FIX16 y, int n)
{
    double a = x;
    double b = y;
    double c = 1 << n;
    FIX16 z = (a*c)/b;

    return z;
}

inline FIX16 fixlen(FIX16 x, FIX16 y)
{
    double a = x;
    double b = y;
    FIX16 z = sqrt(a*a + b*b);

    return z;
}

//-----------------------------------------------------------------------     
//
// Rightmost one function: rmo(val)
//   Returns the little-endian bit number (0 = LSB, 31 = MSB) of
//   the rightmost one (least-significant nonzero bit) in 32-bit
//   parameter val; val must be nonzero, or the function faults.
//
// Leftmost one function: lmo(val)
//   Returns the little-endian bit number (0 = LSB, 31 = MSB) of
//   the leftmost one (least-significant nonzero bit) in 32-bit
//   parameter val; val must be nonzero, or the function faults.
//
//-----------------------------------------------------------------------

inline int rmo(unsigned int val)
{
    int n, m = 31;

    assert(val != 0);
    for (n = 16; n; n >>= 1)
        if (val << n)
        {
            val <<= n;
            m -= n;
        }

    return m;
}

inline int lmo(unsigned int val)
{
    int n, m = 0;

    assert(val != 0);
    for (n = 16; n; n >>= 1)
        if (val >> n)
        {
            val >>= n;
            m |= n;
        }

    return m;
}

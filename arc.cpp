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
// arc.cpp:
//   Path manager member functions for constructing ellipses, elliptic
//   arcs, elliptic splines, and rounded rectangles
//
//---------------------------------------------------------------------

#include <stdlib.h>
#include <math.h>
#include "shapepri.h"

namespace {

    //-------------------------------------------------------------------
    //
    // Returns the approximate length of vector (x,y). See the analysis
    // of this approximation by Alan Paeth in Graphics Gems I, p. 427.
    //
    //-------------------------------------------------------------------

    inline FIX16 vlen(FIX16 x, FIX16 y)
    {
        x = abs(x);
        y = abs(y);
        return x + y - min(x,y)/2;
    }

    //-------------------------------------------------------------------
    //
    // Returns the approximate length of the radius of the major
    // auxiliary circle for the origin-centered ellipse specified by
    // conjugate diameter end points v1 and v2. This radius is half the
    // length of the ellipse's major axis.
    //
    //-------------------------------------------------------------------

    FIX16 auxradius(VERT16 v1, VERT16 v2)
    {
        const int shift[] = { 1, 4, 6, /* 8, 10, 12 */ };
        VERT16 v3;
        FIX16 r;
    
        // Start with points v1, v2, and v3 on the ellipse boundary
        v3.x = -v1.x;
        v3.y = -v1.y;
    
        // With each iteration of this for-loop, points v1 and v3 more
        // narrowly bracket the ellipse's major axis
        for (int n = 0; n < ARRAY_LEN(shift); ++n)
        {
            FIX16 d12 = vlen(v1.x - v2.x, v1.y - v2.y);
            FIX16 d23 = vlen(v2.x - v3.x, v2.y - v3.y);
    
            if (d12 < d23)
                v3 = v2;
            else
                v1 = v2;
    
            v2.x = (v1.x + v3.x)/2;  // set v2 to midpoint of v1 and v3
            v2.y = (v1.y + v3.y)/2;
            v2.x += v2.x >> shift[n];  // extend v2 to edge of ellipse
            v2.y += v2.y >> shift[n];
        }
        r = vlen(v2.x, v2.y);
        return r;
    }
}

//---------------------------------------------------------------------
//
// Private function: Generates a series of points approximating the
// specified elliptic arc. Parameter v0 specifies the x-y coordinates
// at the center of the ellipse. Parameters v1, v2, and v3 are points
// on the ellipse, and are specified as center-relative coordinates--
// that is, as displacements from v0. Parameters v1 and v3 are the
// start and end points of the arc. Parameters v1 and v2 are the end
// points of a pair of conjugate diameters of the ellipse. Parameter
// asweep is the angle swept out by the arc. If parameter asweep is
// positive, the arc sweeps in the direction from v1 to v2; otherwise,
// it sweeps in the opposite direction. This code is based on the
// quarter-ellipse algorithm from Graphics Gems III.
//
//----------------------------------------------------------------------

void PathMgr::FlattenArc(const VERT16& v0, const VERT16& v1, VERT16 v2, VERT16 v3, FIX16 asweep)
{
    int k, nverts;
    FIX16 vx, ux, vy, uy, w;
    FIX16 r = auxradius(v1, v2);  // 1/2 length of ellipse major axis
    FIX16 e1 = r/2, e2 = r/4;     // flatness error components

    // Find shift amount k for which arc segments meet flatness spec
    for (k = 1; k < MAXLEVELS; ++k)
    {
        e1 = e1/4;
        e2 = e2/16;
        if (e1 + e2 <= _flatness)
            break;
    }

    // Convert negative angle to positive angle in opposite direction
    if (asweep < 0)
    {
        v2.x = -v2.x;
        v2.y = -v2.y;
        asweep = -asweep;
    }

    // Count number of vertexes needed to smoothly approximate arc
    nverts = (asweep << k) >> 16;

    // Generate a series of line segments to represent the arc
    vx = v1.x;
    ux = v2.x;
    vy = v1.y;
    uy = v2.y;
    ux -= (w = ux >> (2*k + 3));   // cancel 2nd-order error
    ux -= (w >>= (2*k + 4));       // cancel 4th-order error
    ux -= w >> (2*k + 3);          // cancel 6th-order error
    ux += vx >> (k + 1);           // cancel 1st-order error
    uy -= (w = uy >> (2*k + 3));   // cancel 2nd-order error
    uy -= (w >>= (2*k + 4));       // cancel 4th-order error
    uy -= w >> (2*k + 3);          // cancel 6th-order error
    uy += vy >> (k + 1);           // cancel 1st-order error
    for (int i = 0; i < nverts; ++i)
    {
        ux -= vx >> k;
        vx += ux >> k;
        uy -= vy >> k;
        vy += uy >> k;
        v2.x = v0.x + vx;
        v2.y = v0.y + vy;
        PathCheck(++_cpoint);
        *_cpoint = v2;
    }

    // Just in case the current point, *_cpoint, falls slightly short of the
    // exact arc end point, v3, append a final line segment extending to v3.
    v3.x += v0.x;
    v3.y += v0.y;
    PathCheck(++_cpoint);
    *_cpoint = v3;
}

//---------------------------------------------------------------------
//
// Public function: Adds a rotated ellipse to the current path. The
// ellipse is specified by the three points v0, v1, and v3. Parameter v0
// specifies the x-y coordinates at the center of the ellipse.
// Parameters v1 and v2 specify the x-y coordinates at the end points of
// two conjugate diameters of the ellipse. Before constructing the
// ellipse, the function ends any previous figure that might have been
// under construction in the current path. Then the function adds the
// ellipse to the path as a new, closed figure. Finally, the function
// starts a new, empty figure in the same path (so that, on return, the
// current point is undefined).
//
//----------------------------------------------------------------------

void PathMgr::Ellipse(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2)
{
    VERT16 v[3];
    
    EndFigure();
    _cpoint = _fpoint;
    _cpoint->x = v1.x << _fixshift;
    _cpoint->y = v1.y << _fixshift;
    v[0].x = v0.x << _fixshift;
    v[0].y = v0.y << _fixshift;
    v[1].x = (v1.x - v0.x) << _fixshift;
    v[1].y = (v1.y - v0.y) << _fixshift;
    v[2].x = (v2.x - v0.x) << _fixshift;
    v[2].y = (v2.y - v0.y) << _fixshift;
    FlattenArc(v[0], v[1], v[2], v[1], 2*FIX_PI);  // flatten ellipse
    CloseFigure();
}

//---------------------------------------------------------------------
//
// Public function: Appends an elliptic arc to the current path. 
// Point v0 is the center of the ellipse, and v1 and v2 are the end
// points of a pair of conjugate diameters of the ellipse. Parameter
// aStart is the starting angle of the arc, and parameter aSweep is the
// angle swept out by the arc. Both angles are specified in radians of
// elliptic arc, and both can have positive or negative values. The
// starting angle is specified relative to v1, and is positive in the
// direction of v2. The sweep angle is positive in the same direction
// as the start angle. If, on entry to this function, the current point
// is undefined, the starting point of the arc becomes the first point
// in a new figure. Otherwise, the function inserts a line segment
// connecting the current point to the starting point of the arc. On
// return from this function, the current point is set to the end point
// of the arc. 
//
//----------------------------------------------------------------------

void PathMgr::EllipticArc(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2, float aStart, float aSweep)
{  
    float sina = sin(aStart);
    float cosa = cos(aStart);
    float sinb = sin(aSweep);
    float cosb = cos(aSweep);
    float shift = 1 << _fixshift;
    float x1, y1, x2, y2;
    FIX16 asweep = 65536.0*aSweep;
    VERT16 va, vb, v[4];

    // Convert center coordinates to 16.16 fixed-point format
    v[0].x = v0.x << _fixshift;
    v[0].y = v0.y << _fixshift;

    // Rotate conjugate diameter end points v1 and v2 around the
    // perimeter of the ellipse by aStart radians so that the
    // first end point coincides with the arc starting point.
    va.x = v1.x - v0.x;
    va.y = v1.y - v0.y;
    vb.x = v2.x - v0.x;
    vb.y = v2.y - v0.y;
    x1 = va.x*cosa + vb.x*sina;
    y1 = va.y*cosa + vb.y*sina;
    x2 = vb.x*cosa - va.x*sina;
    y2 = vb.y*cosa - va.y*sina;
    v[1].x = x1*shift;
    v[1].y = y1*shift;
    v[2].x = x2*shift;
    v[2].y = y2*shift;

    // Calculate the arc end point by rotating aSweep radians around
    // the ellipse starting from the arc starting point at (x1,y1).
    v[3].x = (x1*cosb + x2*sinb)*shift;
    v[3].y = (y1*cosb + y2*sinb)*shift;

    // Set the current point to the arc starting point, and call the
    // FlattenArc function to add the points in the arc to the path.
    if (_cpoint == 0)
        _cpoint = _fpoint;
    else
        PathCheck(++_cpoint);

    _cpoint->x = v[0].x + v[1].x;
    _cpoint->y = v[0].y + v[1].y;
    FlattenArc(v[0], v[1], v[2], v[3], asweep);
}

//----------------------------------------------------------------------
//
// Public function: Appends an elliptic spline to the current figure
// in the current path. The spline is a quarter of an ellipse; that is,
// it consists of PI/2 radians of elliptic arc. The control polygon
// for the spline is defined by three points: the start point (the
// current point), the end point (parameter v2), and the control point
// (v1). On return from this function, the new current point is v2.
//
//----------------------------------------------------------------------

bool PathMgr::EllipticSpline(const SGPoint& v1, const SGPoint& v2)
{
    VERT16 v[3];

    if (_cpoint == 0)
    {
        assert(_cpoint != 0);
        return false;
    }

    // Get 3 vertexes for spline control polygon
    v[1] = *_cpoint;
    v[0].x = v1.x << _fixshift;
    v[0].y = v1.y << _fixshift;
    v[2].x = v2.x << _fixshift;
    v[2].y = v2.y << _fixshift;

    // Get center of ellipse
    v[0].x = v[1].x + v[2].x - v[0].x;
    v[0].y = v[1].y + v[2].y - v[0].y;

    // Convert start/end points to center-relative coordinates
    v[1].x -= v[0].x;
    v[1].y -= v[0].y;
    v[2].x -= v[0].x;
    v[2].y -= v[0].y;
    FlattenArc(v[0], v[1], v[2], v[2], FIX_PI/2);   // flatten spline
    return true;
}

//----------------------------------------------------------------------
//
// Public function: Appends a series of elliptic splines to the current 
// figure in the current path. The current point is taken as the
// starting point for the first spline, and the first two points in
// array xy are taken as the control point and end point, respectively,
// for the first spline. Each subsequent pair of points in array xy
// defines an additional spline. The end point of each spline is taken
// as the starting point for the next spline. Parameter npts is the
// number of points in array xy. On return from this function, the last
// point in array xy is the new current point.
//
//----------------------------------------------------------------------

bool PathMgr::PolyEllipticSpline(int npts, const SGPoint xy[])
{
    const SGPoint *pxy = &xy[0];

    if (_cpoint == 0 || xy == 0)
    {
        assert(_cpoint != 0);
        assert(xy != 0);
        return false;
    }
    for (int i = 0; i < npts; i += 2)
    {
        EllipticSpline(pxy[0], pxy[1]);
        pxy = &pxy[2];
    }
    return true;
}

//----------------------------------------------------------------------
//
// Public function: Adds a rectangle with rounded corners to the
// current path. Corners are rounded with circular or elliptic arcs.
// The rectangle position and dimensions are given by parameter rect.
// Parameter round specifies the x and y displacements of the arc start
// and end points from each corner of the rectangle. On entry, this
// function starts a new figure in the current path, and then adds the
// rounded rectangle to this path as a closed figure. Finally, the
// function starts a new, empty figure in the same path (so that the
// current point is undefined). Rotation direction = clockwise.
//
//----------------------------------------------------------------------

void PathMgr::RoundedRectangle(const SGRect& rect, const SGPoint& round)
{
    if (round.x == 0 || round.y == 0)
    {
        Rectangle(rect);
        return;
    }

    // Convert input parameters to internal fixed-point format
    FIX16 xmin = rect.x << _fixshift;
    FIX16 ymin = rect.y << _fixshift;
    FIX16 xmax = (rect.x + rect.w) << _fixshift;
    FIX16 ymax = (rect.y + rect.h) << _fixshift;
    FIX16 xround = round.x << _fixshift;
    FIX16 yround = round.y << _fixshift;

    // Add top-left rounded corner to path
    VERT16 v[3];
    v[0].x = xmin + xround;
    v[0].y = ymin + yround;
    v[1].x = -xround;
    v[1].y = 0;
    v[2].x = 0;
    v[2].y = -yround;
    EndFigure();
    _cpoint = _fpoint;
    _cpoint->x = xmin;
    _cpoint->y = ymin + yround;
    FlattenArc(v[0], v[1], v[2], v[2], FIX_PI/2);

    // Use symmetry to copy top-left corner to other 3 corners
    int count = _cpoint - _fpoint + 1;
    VERT16 *ptr = _cpoint;
    int i;
    for (i = 0; i < count; ++i)
    {
        PathCheck(++_cpoint);
        _cpoint->x = xmin + xmax - ptr->x;
        _cpoint->y = ptr->y;
        --ptr;
    }
    ptr = _cpoint;
    for (i = 0; i < 2*count; ++i)
    {
        PathCheck(++_cpoint);
        _cpoint->x = ptr->x;
        _cpoint->y = ymin + ymax - ptr->y;
        --ptr;
    }
    CloseFigure();
}
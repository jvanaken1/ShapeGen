/*
  Copyright (C) 2019-2024 Jerry R. VanAken

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

#include <math.h>
#include "shapepri.h"

namespace {
    //-------------------------------------------------------------------
    //
    // Returns the approximate length of vector (x,y). The error in the
    // return value falls within the range -2.8 to +0.78 percent.
    //
    //-------------------------------------------------------------------

    FIX16 VLen(FIX16 x, FIX16 y)
    {
        if (x < 0) x = -x;
        if (y < 0) y = -y;
        if (x > y)
            return x + max(y/8, y/2 - x/8);

        return y + max(x/8, x/2 - y/8);
    }

    //-------------------------------------------------------------------
    //
    // Returns the approximate length of the radius of the major
    // auxiliary circle for the origin-centered ellipse specified by
    // conjugate diameter end points (xP,yP) and (xQ,yQ). This radius
    // is half the length of the ellipse's major axis. The error in
    // the return value falls within the range -1.3 to +8.4 percent.
    //
    //-------------------------------------------------------------------

    FIX16 AuxRadius(FIX16 xP, FIX16 yP, FIX16 xQ, FIX16 yQ)
    {
        FIX16 dP = VLen(xP, yP);
        FIX16 dQ = VLen(xQ, yQ);
        FIX16 dA = VLen(xP + xQ, yP + yQ);
        FIX16 dB = VLen(xP - xQ, yP - yQ);
        FIX16 r1 = max(dP, dQ);
        FIX16 r2 = max(dA, dB);

        return max(r1 + r1/16, r2 - r2/4);
    }

    //-------------------------------------------------------------------
    //
    // The inner loop of Marvin Minsky's circle generator. This function
    // rotates point (u,v) around the origin by an approximate angular
    // increment of epsilon = 1/2^k radians, so that multiplication of a
    // value x by epsilon is calculated as x*alpha = x >> k.
    //
    //-------------------------------------------------------------------

    inline void CircleGen(FIX16& u, FIX16& v, int k)
    {
        u -= v >> k;
        v += u >> k;
    }

    //-------------------------------------------------------------------
    //
    // Coordinates (u0,v0) specify an initial point on a circle that
    // is to be drawn by Minsky's circle generator. The InitialValue
    // function returns a value U0 to substitute for u0; this substi-
    // tution that will cancel out most of the error in the v
    // coordinates produced by the circle generator (and turn it into
    // a precise sine wave generator).
    //
    //-------------------------------------------------------------------

    inline FIX16 InitialValue(FIX16 u0, FIX16 v0, int k)
    {
        int shift = 2*k + 3;
        FIX16 w = u0 >> shift;
        FIX16 U0 = u0 - w + (v0 >> (k + 1));

        w >>= shift + 1;
        U0 -= w;
        w >>= shift;
        U0 -= w;
        return U0;
    }
}

//-------------------------------------------------------------------
//
// Private function: Returns the exponent k used to calculate the
// approximate angular increment between points plotted on an
// ellipse centered at the origin. The ellipse is specified by
// conjugate diameter end points (xP,yP) and (xQ,yQ). The
// corresponding angular increment is alpha = 1/2^k radians, and
// multiplication of a value x by alpha is calculated as x >> k.
//
//-------------------------------------------------------------------

int PathMgr::AngularInc(FIX16 xP, FIX16 yP, FIX16 xQ, FIX16 yQ)
{
    FIX16 r = AuxRadius(xP, yP, xQ, yQ);
    FIX16 err2 = r >> 3;    // 2nd-order term
    FIX16 err4 = r >> 7;    // 4th-order term

    for (int k = 0; k < KMAX; ++k)
    {
        if (_flatness >= err2 + err4)
            return k;

        err2 >>= 2;
        err4 >>= 4;
    }
    return KMAX;
}

//---------------------------------------------------------------------
//
// Private function: Core ellipse algorithm. Generates a series of
// points along the specified elliptic arc. Parameters (xC,yC) are
// the x-y coordinates at the center of the ellipse. Parameters
// (xP,yP) and (xQ,yQ) are the center-relative coordinates of two end
// points, P and Q, of a pair of conjugate diameters of the ellipse.
// Center-relative coordinates are specified as x-y offsets from the
// ellipse center. The arc starting point is (xP,yP). Parameter asweep
// is the angle swept out by the arc. If parameter asweep is positive,
// the arc sweeps in the direction from (xP,yP) to (xQ,yQ). This code
// is based on the quarter-ellipse algorithm from Graphics Gems III.
//
//----------------------------------------------------------------------

void PathMgr::EllipseCore(FIX16 xC, FIX16 yC, FIX16 xP, FIX16 yP,
                          FIX16 xQ, FIX16 yQ, FIX16 sweep)
{
    int k = AngularInc(xP, yP, xQ, yQ);
    int count = sweep >> (16 - k);

    xQ = InitialValue(xQ, xP, k);
    yQ = InitialValue(yQ, yP, k);
    for (int i = 0; i < count; ++i)
    {
        CircleGen(xQ, xP, k);
        CircleGen(yQ, yP, k);
        PathCheck(++_cpoint);
        _cpoint->x = xC + xP;
        _cpoint->y = yC + yP;
    }
}

//---------------------------------------------------------------------
//
// Public function: Adds a rotated ellipse to the current path. The
// ellipse is specified by the three points v0, v1, and v3. Parameter
// v0 specifies the x-y coordinates at the center of the ellipse.
// Parameters v1 and v2 specify the x-y coordinates at the end points
// of two conjugate diameters of the ellipse. Before constructing the
// ellipse, the function finalizes any previous figure that might have
// been under construction in the current path. Then the function adds
// the ellipse to the path as a new, closed figure. Finally, the
// function starts a new, empty figure in the same path (so that, on
// return, the current point is undefined).
//
//----------------------------------------------------------------------

void PathMgr::Ellipse(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2)
{
    FIX16 xC = v0.x << _fixshift;
    FIX16 yC = v0.y << _fixshift;
    FIX16 xP = (v1.x - v0.x) << _fixshift;
    FIX16 yP = (v1.y - v0.y) << _fixshift;
    FIX16 xQ = (v2.x - v0.x) << _fixshift;
    FIX16 yQ = (v2.y - v0.y) << _fixshift;

    EndFigure();
    _cpoint = _fpoint;
    _cpoint->x = v1.x << _fixshift;
    _cpoint->y = v1.y << _fixshift;
    EllipseCore(xC, yC, xP, yP, xQ, yQ, FIX_2PI);
    CloseFigure();
}

//---------------------------------------------------------------------
//
// Public function: Appends an elliptic arc to the current path.
// Point v0 is the center of the ellipse, and v1 and v2 are the end
// points of a pair of conjugate diameters of the ellipse. Parameter
// astart is the starting angle of the arc, and parameter asweep is the
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

void PathMgr::EllipticArc(const SGPoint& v0, const SGPoint& v1, const SGPoint& v2,
                          float astart, float asweep)
{
    FIX16 xC = v0.x << _fixshift;
    FIX16 yC = v0.y << _fixshift;
    FIX16 xP = (v1.x - v0.x) << _fixshift;
    FIX16 yP = (v1.y - v0.y) << _fixshift;
    FIX16 xQ = (v2.x - v0.x) << _fixshift;
    FIX16 yQ = (v2.y - v0.y) << _fixshift;
    float cosb, sinb;
    FIX16 swangle;

    if (astart != 0)
    {
        // Generate new conjugate diameter end points P' and Q'
        // by rotating points P and Q by starting angle astart
        float cosa = cos(astart);
        float sina = sin(astart);
        FIX16 x = xP*cosa + xQ*sina;
        FIX16 y = yP*cosa + yQ*sina;

        xQ = xQ*cosa - xP*sina;
        yQ = yQ*cosa - yP*sina;
        xP = x;
        yP = y;
    }

    // Convert negative angle to positive angle in opposite direction
    if (asweep < 0)
    {
        xQ = -xQ;
        yQ = -yQ;
        asweep = -asweep;
    }
    swangle = 65536.0*asweep;

    // Set the current point to the arc starting point, and call the
    // EllipseCore function to add the points in the arc to the path.
    if (_cpoint == 0)
        _cpoint = _fpoint;
    else
        PathCheck(++_cpoint);

    _cpoint->x = xC + xP;
    _cpoint->y = yC + yP;
    EllipseCore(xC, yC, xP, yP, xQ, yQ, swangle);

    // Append arc end point to path
    cosb = cos(asweep);
    sinb = sin(asweep);
    xP = xP*cosb + xQ*sinb;
    yP = yP*cosb + yQ*sinb;
    PathCheck(++_cpoint);
    _cpoint->x = xC + xP;
    _cpoint->y = yC + yP;
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
    if (_cpoint == 0)
    {
        assert(_cpoint != 0);
        return false;
    }

    FIX16 xP = _cpoint->x;
    FIX16 yP = _cpoint->y;
    FIX16 xQ = v2.x << _fixshift;
    FIX16 yQ = v2.y << _fixshift;
    FIX16 xC = xP + xQ - (v1.x << _fixshift);
    FIX16 yC = yP + yQ - (v1.y << _fixshift);

    EllipseCore(xC, yC, xP-xC, yP-yC, xQ-xC, yQ-yC, FIX_PI/2);
    PathCheck(++_cpoint);
    _cpoint->x = xQ;
    _cpoint->y = yQ;
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

bool PathMgr::PolyEllipticSpline(const SGPoint xy[], int npts)
{
    const SGPoint *pxy = &xy[0];

    if (_cpoint == 0 || npts < 0 || xy == 0)
    {
        assert(_cpoint != 0 && npts >= 0 && xy != 0);
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
    FIX16 xC = xmin + xround;
    FIX16 yC = ymin + yround;
    FIX16 xP = -xround;
    FIX16 yP = 0;
    FIX16 xQ = 0;
    FIX16 yQ = -yround;

    // Add points in top-left rounded corner to path
    EndFigure();
    _cpoint = _fpoint;
    _cpoint->x = xmin;
    _cpoint->y = ymin + yround;
    EllipseCore(xC, yC, xP, yP, xQ, yQ, FIX_PI/2);
    PathCheck(++_cpoint);
    _cpoint->x = xC + xQ;
    _cpoint->y = yC + yQ;

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

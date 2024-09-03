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
// curve.cpp:
//   Path manager member functions for generating Bezier spline
//   curves -- both quadratic and cubic
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
            return max(x + y/8, x + y/2 - x/8);

        return max(y + x/8, y + x/2 - y/8);
    }
}

//--------------------------------------------------------------------
//
// Private function: Returns a true/false value indicating whether
// the quadratic Bezier curve segment specified by the 3-vertex
// control polygon (v0,v1,v2) is flat enough to be represented by a
// line segment from v0 to v2.
//
//--------------------------------------------------------------------

bool PathMgr::IsFlatQuadratic(const VERT16 v[3])
{
    FIX16 dx = v[0].x - 2*v[1].x + v[2].x;
    FIX16 dy = v[0].y - 2*v[1].y + v[2].y;
    FIX16 error = VLen(dx, dy)/4;

    return (error <= _flatness);
}

//---------------------------------------------------------------------
//
// Public function: Appends a quadratic Bezier curve (a parabola) to
// the current figure (aka subpath). The current point serves as the
// first point in the spline's 3-point control polygon. Parameters v1
// and v2 supply the two remaining points. On return, the point
// specified by v2 is the new current point. This function uses the
// classic de Casteljau algorithm to subdivide the Bezier curve into
// segments that meet the tolerance specified by the _flatness member.
// On entry, the current figure must not be empty (that is, the
// current point must be defined) or the function faults.
//
//----------------------------------------------------------------------

bool PathMgr::Bezier2(const SGPoint& v1, const SGPoint& v2)
{
    VERT16 vstack[2*MAXLEVELS];  // stack for polygon vertices
    VERT16 *pvstk = &vstack[0];  // vertex stack pointer
    int lstack[MAXLEVELS];       // stack for level numbers
    int *plstk = &lstack[0];     // level stack pointer
    int level = 0;               // current subdivision level
    VERT16 v[2+1][2+1];          // control polygon vertices

    if (_cpoint == 0)
    {
        assert(_cpoint != 0);
        return false;
    }

    // Get the three vertices for Bezier control polygon ABC
    v[0][0] = *_cpoint;             // A
    v[0][1].x = v1.x << _fixshift;  // B
    v[0][1].y = v1.y << _fixshift;
    v[0][2].x = v2.x << _fixshift;  // C
    v[0][2].y = v2.y << _fixshift;

    // Continue to subdivide Bezier control polygon ABC until the
    // flatness of each curve segment is within the specified tolerance
    for (;;)
    {
        while (!IsFlatQuadratic(v[0]) && level < MAXLEVELS)
        {
            // Subdivide control polygon ABC into ADF and FEC
            for (int j = 1; j <= 2; ++j)
                for (int i = 0; i <= 2 - j; ++i)
                {
                    v[j][i].x = (v[j-1][i].x + v[j-1][i+1].x)/2;
                    v[j][i].y = (v[j-1][i].y + v[j-1][i+1].y)/2;
                }

            // Push level, and vertices EC onto stack for later
            *plstk++ = ++level;
            *pvstk++ = v[0][2];  // C
            *pvstk++ = v[1][1];  // E

            // Define new control polygon ABC for next iteration
            v[0][1] = v[1][0];  // B
            v[0][2] = v[2][0];  // C
        }

        // Represent flattened curve segment ABC by its chord, AC
        PathCheck(++_cpoint);
        *_cpoint = v[0][2];  // C

        // If stack is empty, we're done
        if (plstk == &lstack[0])
            break;

        // Pop level, and vertices BC from stack
        level = *--plstk;
        v[0][0] = v[0][2];   // A
        v[0][1] = *--pvstk;  // B
        v[0][2] = *--pvstk;  // C
    }
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Appends a series of connected quadratic Bezier
// curve segments to the current figure (aka subpath). Array 'xy'
// contains the points in the control polygons for the curves.
// Parameter 'npts' specifies the number of elements in the xy array.
// The current point is taken as the first point in the 3-point
// control polygon for the first curve, and the two remaining points
// are specified by the first two points in the xy array. Thereafter,
// two additional array elements are needed to specify each subsequent
// curve. The last point in the final curve then becomes the new
// current point.
//
//----------------------------------------------------------------------

bool PathMgr::PolyBezier2(const SGPoint xy[], int npts)
{
    const SGPoint *pxy = xy;

    if (_cpoint == 0 || npts < 0 || xy == 0)
    {
        assert(_cpoint != 0 && npts >= 0 && xy != 0);
        return false;
    }
    for (int i = 0; i < npts; i += 2)
    {
        Bezier2(pxy[0], pxy[1]);
        pxy = &pxy[2];
    }
    return true;
}

//--------------------------------------------------------------------
//
// Private function: Returns a true/false value indicating whether
// the cubic Bezier curve segment specified by the 4-vertex control
// polygon (v0,v1,v2,v3) is flat enough to be represented by a line
// segment from v0 to v3. This function is similar to the flatness
// test described by Roger Willcocks (www.rops.org) but uses fixed-
// point arithmetic instead of floating-point.
//
//--------------------------------------------------------------------

bool PathMgr::IsFlatCubic(const VERT16 v[4])
{
    FIX16 ux = 2*(v[1].x - v[0].x) + v[1].x - v[3].x;
    FIX16 uy = 2*(v[1].y - v[0].y) + v[1].y - v[3].y;
    FIX16 vx = 2*(v[2].x - v[3].x) + v[2].x - v[0].x;
    FIX16 vy = 2*(v[2].y - v[3].y) + v[2].y - v[0].y;
    FIX16 uerr = VLen(ux, uy);
    FIX16 verr = VLen(vx, vy);
    FIX16 error = max(uerr, verr)/4;

    return (error <= _flatness);
}

//---------------------------------------------------------------------
//
// Public function: Appends a cubic Bezier curve to the current figure
// (aka subpath). The current point serves as the first point in the
// curve's 4-point control polygon. Parameters v1, v2, and v3 supply
// the three remaining points. On return, the point specified by v3 is
// the new current point. This function uses the classic de Casteljau
// algorithm to subdivide the Bezier curve into segments that meet the
// tolerance specified by the _flatness member. On entry, the current
// figure must not be empty (that is, the current point must be
// defined) or the function faults.
//
//----------------------------------------------------------------------

bool PathMgr::Bezier3(const SGPoint& v1, const SGPoint& v2, const SGPoint& v3)
{
    VERT16 vstack[3*MAXLEVELS];  // stack for polygon vertices
    VERT16 *pvstk = &vstack[0];  // vertex stack pointer
    int lstack[MAXLEVELS];       // stack for level numbers
    int *plstk = &lstack[0];     // level stack pointer
    int level = 0;               // current subdivision level
    VERT16 v[3+1][3+1];          // control polygon vertices

    if (_cpoint == 0)
    {
        assert(_cpoint != 0);
        return false;
    }

    // Get the four vertices for Bezier control polygon ABCD
    v[0][0] = *_cpoint;             // A
    v[0][1].x = v1.x << _fixshift;  // B
    v[0][1].y = v1.y << _fixshift;
    v[0][2].x = v2.x << _fixshift;  // C
    v[0][2].y = v2.y << _fixshift;
    v[0][3].x = v3.x << _fixshift;  // D
    v[0][3].y = v3.y << _fixshift;

    // Continue to subdivide control polygon ABCD until the flatness
    // of each curve segment falls within the specified tolerance
    for (;;)
    {
        while (!IsFlatCubic(v[0]) && level < MAXLEVELS)
        {
            // Subdivide control polygon ABCD into AEHJ and JIGD
            for (int j = 1; j <= 3; ++j)
                for (int i = 0; i <= 3 - j; ++i)
                {
                    v[j][i].x = (v[j-1][i].x + v[j-1][i+1].x)/2;
                    v[j][i].y = (v[j-1][i].y + v[j-1][i+1].y)/2;
                }

            // Push level, and vertices IGD onto stack for later
            *plstk++ = ++level;
            *pvstk++ = v[0][3];  // D
            *pvstk++ = v[1][2];  // G
            *pvstk++ = v[2][1];  // I

            // Define new control polygon ABCD for next iteration
            v[0][1] = v[1][0];  // B
            v[0][2] = v[2][0];  // C
            v[0][3] = v[3][0];  // D
        }

        // Represent flattened curve segment ABCD by its chord, AD
        PathCheck(++_cpoint);
        *_cpoint = v[0][3];  // D

        // If stack is empty, we're done
        if (plstk == &lstack[0])
            break;

        // Pop level, and vertices BCD from stack
        level = *--plstk;
        v[0][0] = v[0][3];   // A
        v[0][1] = *--pvstk;  // B
        v[0][2] = *--pvstk;  // C
        v[0][3] = *--pvstk;  // D
    }
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Appends a series of connected cubic Bezier curve
// segments to the current figure (aka subpath). Array 'xy' contains
// the points in the control polygons for the curves. Parameter 'npts'
// specifies the number of elements in the xy array. The current point
// is taken as the first point in the 4-point control polygon for the
// first curve, and the three remaining points are specified by the
// first three points in the xy array. Thereafter, three additional
// array elements are needed to specify each subsequent curve. The
// last point in the final curve then becomes the new current point.
//
//----------------------------------------------------------------------

bool PathMgr::PolyBezier3(const SGPoint xy[], int npts)
{
    const SGPoint *pxy = xy;

    if (_cpoint == 0 || npts < 0 || xy == 0)
    {
        assert(_cpoint != 0 && npts >= 0 && xy != 0);
        return false;
    }
    for (int i = 0; i < npts; i += 3)
    {
        Bezier3(pxy[0], pxy[1], pxy[2]);
        pxy = &pxy[3];
    }
    return true;
}


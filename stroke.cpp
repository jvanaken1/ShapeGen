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
// stroke.cpp:
//   Path manager member functions for constructing stroked paths
//
//---------------------------------------------------------------------

#include <math.h>
#include "shapepri.h"

namespace {
    // By default, use the acos function in math.h
#if 0
    //-----------------------------------------------------------------
    //
    // Arccosine approximation from Abramowitz & Stegun, page 81.
    // Input range is -1 <= x <= 1. Output range is 0 <= angle <= pi.
    //
    //-----------------------------------------------------------------

    float acos(float x)
    {
        const float PI = 3.14159265358979;
        const float a0 =  1.5707288;
        const float a1 = -0.2121144;
        const float a2 =  0.0742610;
        const float a3 = -0.0187293;
        float val = a3;
        bool negate = x < 0;

        if (negate)
            x = -x;

        val = val*x + a2;
        val = val*x + a1;
        val = val*x + a0;
        val *= sqrt(1.0 - x);
        return (negate) ? PI - val : val;
    }
#endif
    //-----------------------------------------------------------------
    //
    // Return the angle, in radians, between unit vectors u and v
    //
    //-----------------------------------------------------------------
    FIX16 GetAngle(const XY& u, const XY& v)
    {
        float cosine = u.x*v.x + u.y*v.y;
        float theta = acos(cosine);
        FIX16 angle = 65536*theta;

        return angle;
    }

    //-----------------------------------------------------------------
    //
    // Flip vector v around to point in the opposite direction
    //
    //-----------------------------------------------------------------
    inline void FlipVector(VERT16& v)
    {
        v.x = -v.x;
        v.y = -v.y;
    }

    //-----------------------------------------------------------------------
    //
    // Fixed-point multiply function: Fix16Mult(x, y)
    //   Multiplies together two 32-bit, 16.16 fixed-point values, x and y,
    //   both of which have 16 bits of fraction. Returns the 32-bit product
    //   in the same fixed-point format.
    //
    //-----------------------------------------------------------------------
    inline FIX16 Fix16Mult(FIX16 x, FIX16 y)
    {
        const float c = 1.0f/(1 << 16);
        float a = x, b = y;
        FIX16 z = (a*b)*c;
        return z;
    }
}

//---------------------------------------------------------------------
//
// Public function: Sets the stroked line width. Parameter width
// specifies the line width in pixels. The default line width is
// 4.0 pixels.
//
//----------------------------------------------------------------------

float PathMgr::SetLineWidth(float width)
{
    float oldwidth = _linewidth/65536.0f;

    if (width < 0)
    {
        assert(width >= 0);
        return 0;
    }
    _linewidth = 65536*width;  // convert to 16.16 fixed-point format
    return oldwidth;
}

//---------------------------------------------------------------------
//
// Public function: Sets the line end-cap style for stroked lines.
// Parameter capstyle is restricted to the values LINEEND_FLAT (0),
// LINEEND_ROUND (1), and LINEEND_SQUARE (2). If capstyle is set to
// an illegal value, the function faults. The default line end-cap
// style is LINEEND_FLAT.
//
//----------------------------------------------------------------------

LINEEND PathMgr::SetLineEnd(LINEEND capstyle)
{
    LINEEND oldcap = _lineend;

    switch (capstyle)
    {
    case LINEEND_FLAT:
    case LINEEND_ROUND:
    case LINEEND_SQUARE:
        _lineend = capstyle;
        break;
    default:
        assert(0);
        _lineend = LINEEND_DEFAULT;
        break;
    }
    return oldcap;
}

//---------------------------------------------------------------------
//
// Public function: Sets the join style for stroked lines. Parameter
// joinstyle is restricted to the values LINEJOIN_BEVEL (0),
// LINEJOIN_ROUND (1), and LINEJOIN_MITER (2). If joinstyle is set to
// an illegal value, the function faults. The default value is
// LINEJOIN_BEVEL.
//
//----------------------------------------------------------------------

LINEJOIN PathMgr::SetLineJoin(LINEJOIN joinstyle)
{
    LINEJOIN oldjoin = _linejoin;

    switch (joinstyle)
    {
    case LINEJOIN_BEVEL:
    case LINEJOIN_ROUND:
    case LINEJOIN_MITER:
    case LINEJOIN_SVG_MITER:
        _linejoin = joinstyle;
        break;
    default:
        assert(0);
        _linejoin = LINEJOIN_DEFAULT;
        break;
    }
    return oldjoin;
}

//---------------------------------------------------------------------
//
// Public function: Sets the miter limit. Parameter mlim specifies the
// length at which the sharp point of a mitered corner should be
// beveled off. Internally, the mlim value is multiplied by half the
// stroked line width to determine the maximum extent of a mitered
// join. Parameter mlim should be a value greater than or equal to
// 1.0; if less than 1.0, the function clamps it to 1.0. A value of
// mlim = 1.0 cuts off miters at any angle to produce beveled
// joins. The default miter limit value is 10.0. The return value is
// the previous miter limit setting.
//
//---------------------------------------------------------------------

float PathMgr::SetMiterLimit(float mlim)
{
    float oldmlim = _miterlimit/65536.0f;

    mlim = max(mlim, MITERLIMIT_MINIMUM);
    _miterlimit = 65536*mlim;
    _mitercheck = 65536*sqrt(mlim*mlim - 1);
    return oldmlim;
}

//---------------------------------------------------------------------
//
// Public function: Sets the dashed-line pattern used by the
// StrokePath function. The pattern is specified by the dash array,
// which is a zero-terminated character string. The array elements are
// interpreted as a sequence of distances along a stroked path. These
// elements alternately specify the lengths of the dashes and the gaps
// between the dashes. The first element is a dash length, the second
// is a gap length, and so on. Each byte in the dash array is treated
// as an unsigned, 8-bit integer regardless of whether the compiler
// defines type char to be signed or unsigned. Parameter mult is the
// pattern length multiplier. For a given dash array element, the
// length in pixels is determined by multiplying the element by the
// mult parameter. For example, if dash[0] = 4, and mult = 5.2, the
// corresponding dash length is 4*5.2 = 20.8 pixels. The offset
// parameter is the starting offset into the pattern, and is
// specified in the same distance units as the elements in the dash
// array. The StrokePath function starts the dash pattern at this
// offset for each new figure in a path. When the StrokePath function
// reaches the end of the dash array, it resets to the start of the
// array and continues interpreting the array. The maximum length of
// the dash array is 32 elements, not counting the terminating zero.
// The default line pattern is a solid line. This default can always
// be restored by calling SetLineDash with the dash parameter set to
// zero or pointing to an empty string. The function returns true if
// it successfully updates the dash pattern. Otherwise, it returns
// false (after faulting if NDEBUG is undefined).
//
//---------------------------------------------------------------------

bool PathMgr::SetLineDash(char *dash, int offset, float mult)
{
    const int ixmax = ARRAY_LEN(_dasharray) - 1;  // max array index
    bool retval = true;
    int k;

    if (dash == 0 || dash[0] == '\0')
    {
        _dasharray[0] = 0;
        return true;  // solid line (no dash pattern)
    }
    if (offset < 0 || mult <= 0)
    {
        assert(offset >= 0 && mult > 0);
        return false;  // bad parameter value
    }
    for (k = 0; k < ixmax && dash[k] != '\0'; ++k)
    {
        const float toolong = 16384;
        float dashlen = mult*static_cast<unsigned char>(dash[k]);

        if (dashlen >= toolong)
        {
            assert(dashlen < toolong);
            retval = false;
            break;  // excessively long dash
        }
        _dasharray[k] = 65536*dashlen;
    }
    _dasharray[k] = 0;  // terminate internal dash array
    _dashoffset = 65536*mult*offset;
    return retval;
}

//---------------------------------------------------------------------
//
// Private function: Initializes the dashed-line pattern at the start
// of a new figure. Advances to the starting position in the pattern
// specified by the _dashoffset member. This position is described by
// members _pdash (pointer to current _dasharray element), _dashlen
// (remaining length in current _dasharray element), and _dashon (true
// if current _dasharray element is a dash, and false if it's a gap).
//
//----------------------------------------------------------------------

bool PathMgr::InitLineDash()
{
    _dashon = true;
    _pdash = 0;
    if (_dasharray[0] != 0)
    {
        int offset = _dashoffset;

        // Set starting offset into dash pattern array
        _pdash = &_dasharray[0];
        while (offset > *_pdash)
        {
            offset -= *_pdash++;
            _dashon = !_dashon;
            if (*_pdash == 0)
                _pdash = &_dasharray[0];
        }

        // Calculate length remaining in current dash/gap
        _dashlen = *_pdash++ - offset;
    }
    return _dashon;
}

//---------------------------------------------------------------------
//
// Private function: Calculates the length and direction of a directed
// line segment given its starting and ending points, vs and ve.
// Parameters u and a are both output parameters. Output parameter u
// is a unit vector that points in the direction of the line. Output
// parameter a is a vector of length _linewidth/2 that points in the
// same direction as u. The return value is the length (in pixels) of
// the line segment, expressed as a 16.16 fixed-point value.
//
//----------------------------------------------------------------------

FIX16 PathMgr::LineLength(const VERT16& vs, const VERT16& ve, XY *u, VERT16 *a)
{
    float dx = ve.x - vs.x;
    float dy = ve.y - vs.y;
    float len = sqrt(dx*dx + dy*dy);

    assert(len > 0);
    u->x = dx/len;
    u->y = dy/len;
    a->x = u->x*(_linewidth/2);
    a->y = u->y*(_linewidth/2);
    FIX16 length = len;
    return length;
}

//---------------------------------------------------------------------
//
// Private function: Constructs a stroked line segment in accordance
// with the currently selected dashed-line pattern. This function
// handles the dashed-line on/off transitions within the current line
// segment. (Note that the JoinLines function constructs the portion of
// the line segment that follows the last of these transitions). The
// DashedLine function is called only during construction of a stroked
// path with a dashed-line pattern. For a stroked path drawn with a
// solid line, this function is never called -- instead, each line
// segment is constructed in its entirety by the JoinLines function.
// Parameter ve is the end point of the directed line segment. Parameter
// u is a unit vector that points in the direction of this line.
// Parameter a is a vector of length _linewidth/2 that points in the same
// direction as unit vector u. Parameter linelen is the length (in
// pixels) of the line segment.
//
//----------------------------------------------------------------------

void PathMgr::DashedLine(const VERT16& ve, const XY& u, const VERT16& a, FIX16 linelen)
{
    assert(_pdash != 0);
    while (_dashlen <= linelen)
    {
        VERT16 vs, v1, v2;
        FIX16 dx, dy;

        // Calculate displacement to start of next dash or gap
        linelen -= _dashlen;
        dx = linelen*u.x;
        dy = linelen*u.y;
        if (_dashon)
        {
            // Construct a stroked dash of the specified line width
            if (_lineend == LINEEND_SQUARE)
            {
                dx -= a.x;
                dy -= a.y;
            }
            vs.x = ve.x - dx;
            vs.y = ve.y - dy;
            v1.x = vs.x + a.y;
            v1.y = vs.y - a.x;
            v2.x = vs.x - a.y;
            v2.y = vs.y + a.x;
            _edge->AttachEdge(&_vin, &v1);
            _edge->AttachEdge(&v2, &_vout);

            // Cap the end of this dash
            if (_lineend == LINEEND_ROUND)
            {
                VERT16 a2 = a;

                FlipVector(a2);
                _angle = FIX_PI;
                RoundJoin(vs, a, a2);
            }
            else
                _edge->AttachEdge(&v1, &v2);  // flat or square cap
        }
        else
        {
            // Skip over the gap between dashes
            if (_lineend == LINEEND_SQUARE)
            {
                dx += a.x;
                dy += a.y;
            }
            vs.x = ve.x - dx;
            vs.y = ve.y - dy;
            _vin.x = vs.x + a.y;
            _vin.y = vs.y - a.x;
            _vout.x = vs.x - a.y;
            _vout.y = vs.y + a.x;

            // Cap the start of the next dash
            if (_lineend == LINEEND_ROUND)
            {
                VERT16 a1 = a;

                FlipVector(a1);
                _angle = FIX_PI;
                RoundJoin(vs, a1, a);
            }
            else
                _edge->AttachEdge(&_vout, &_vin);  // flat or square cap
        }

        // Advance to next dash on/off transition in this line segment
        if (*_pdash == 0)
            _pdash = _dasharray;

        _dashlen = *_pdash++;
        _dashon = !_dashon;
    }

    // Set up dashed-line parameters for next line segment in the figure
    _dashlen -= linelen;
    if (_dashlen == 0)
    {
        if (*_pdash == 0)
            _pdash = _dasharray;

        _dashlen = *_pdash++;
        _dashon = !_dashon;
    }
}

//----------------------------------------------------------------------
//
// Private function: Adds a round join to the intersection of two line
// segments, or adds a round cap to the start or end point of a stroked
// line segment. Parameter v0 is the start/end point of the line
// segment or segments. Parameters a1 and a2 are vectors of length
// _linewidth/2 that point in the directions of the two directed line
// segments. Before calling this function, set member variable _angle
// to the angle traversed by the arc that forms the round join or
// round cap; _angle is a nonnegative, 16.16 fixed-point value, and
// is expressed in radians. To draw a round cap, set _angle to pi
// radians, and set vectors a1 and a2 to point in opposite directions.
//
//----------------------------------------------------------------------

void PathMgr::RoundJoin(const VERT16& v0, VERT16 a1, VERT16 a2)
{
    VERT16 *p, *q, v1, v2;
    int count;

    // Path should be properly terminated with empty figure
    assert(_cpoint == 0);
    assert(_fpoint == reinterpret_cast<VERT16*>(_figure + 1));
    assert(_figure->isclosed == false);

    // Center-relative arc start point v1 and end point v2
    v1.x =  a1.y;
    v1.y = -a1.x;
    v2.x =  a2.y;
    v2.y = -a2.x;

    // Initialize temp buffer on path stack
    _cpoint = _fpoint;

    // Add arc's starting point to temp path buffer
    _cpoint->x = v0.x + v1.x;
    _cpoint->y = v0.y + v1.y;

    // The EllipseCore function call calculates the points in
    // the arc, adds them to the path, and updates _cpoint
    EllipseCore(v0.x, v0.y, v1.x, v1.y, a1.x, a1.y, _angle);

    // Add arc's ending point to temporary path buffer
    PathCheck(++_cpoint);
    _cpoint->x = v0.x + v2.x;
    _cpoint->y = v0.y + v2.y;

    // Convert points in path to polygonal edges
    p = q = _fpoint;
    count = _cpoint - _fpoint;
    for (int i = 0; i < count; ++i)
        _edge->AttachEdge(p++, ++q);

    // Restore temp buffer on path stack to empty state
    _cpoint = 0;
}

//---------------------------------------------------------------------
//
// Private function: Joins two connected line segments. Parameter v0 is
// the vertex at which the two line segments are joined. Parameters ain
// and aout are vectors of length _linewidth/2 that point in the
// directions of the incoming and outgoing line segments, respectively.
//
//----------------------------------------------------------------------

void PathMgr::JoinLines(const VERT16& v0, const VERT16& ain, const VERT16& aout)
{
    VERT16 v1, v2, v3, v4;

    // Begin by extending normals to v0 from points v1, v2, v3, v4 on
    // the stroked edges of the incoming and outgoing line segments
    v1 = v2 = v3 = v4 = v0;
    v1.x += ain.y;
    v1.y -= ain.x;
    v2.x -= ain.y;
    v2.y += ain.x;
    v3.x += aout.y;
    v3.y -= aout.x;
    v4.x -= aout.y;
    v4.y += aout.x;

    // Connect the edges on the "inside" of the bend that's formed
    // where the incoming and outgoing lines meet to form the join
    FIX16 xprod = Fix16Mult(ain.x, aout.y) - Fix16Mult(ain.y, aout.x);
    if (xprod < 0)
    {
        // Stroke turns left (CCW) at join
        _edge->AttachEdge(&v1, &v0);
        _edge->AttachEdge(&v0, &v3);
    }
    else
    {
        // Stroke turns right (CW) at join
        _edge->AttachEdge(&v4, &v0);
        _edge->AttachEdge(&v0, &v2);
    }

    // Check for bevel or round line join
    if (_linejoin == LINEJOIN_BEVEL || _linejoin == LINEJOIN_ROUND)
    {
        // Construct the line segment that enters the line join
        _edge->AttachEdge(&_vin, &v1);
        _edge->AttachEdge(&v2, &_vout);

        // Construct the bevel or round join
        if (xprod < 0)
        {
            // Stroke turns left (CCW) at join
            if (_linejoin == LINEJOIN_ROUND)
            {
                VERT16 a1 = aout, a2 = ain;

                FlipVector(a1);
                FlipVector(a2);
                RoundJoin(v0, a1, a2);
            }
            else
                _edge->AttachEdge(&v4, &v2);  // bevel
        }
        else
        {
            // Stroke turns right (CW) at join
            if (_linejoin == LINEJOIN_ROUND)
                RoundJoin(v0, ain, aout);
            else
                _edge->AttachEdge(&v1, &v3);  // bevel
        }
        _vin = v3;
        _vout = v4;
        return;  // bevel or round join completed
    }

    // This is a miter join. Determine whether the miter length
    // exceeds the current miter limit.
    float t, numer, denom;

    denom = abs(ain.x + aout.x) + abs(ain.y + aout.y);
    if (fabs(denom) > 0.00025f)  // verify denominator is not close to zero
    {
        numer = abs(ain.x - aout.x) + abs(ain.y - aout.y);
        t = numer/denom;  // linear interpolation param
        FIX16 tfix = 65536*t;
        if (tfix <= _mitercheck)
        {
            // Miter length is within miter limit, so draw full miter
            FIX16 dx = t*(ain.x - aout.x);
            FIX16 dy = t*(ain.y - aout.y);

            if (xprod < 0)
            {
                // Stroke turns left (CCW) at join
                v4.x = (v2.x + v4.x + dx)/2;
                v4.y = (v2.y + v4.y + dy)/2;
                _edge->AttachEdge(&_vin, &v1);
                _edge->AttachEdge(&v4, &_vout);
            }
            else
            {
                // Stroke turns right (CW) at join
                v3.x = (v1.x + v3.x + dx)/2;
                v3.y = (v1.y + v3.y + dy)/2;
                _edge->AttachEdge(&_vin, &v3);
                _edge->AttachEdge(&v2, &_vout);
            }
            _vin = v3;
            _vout = v4;
            return;  // full miter join completed
        }
    }

    // Miter limit exceeded -- we need to snip off the miter point
    if (_linejoin == LINEJOIN_SVG_MITER)
    {
        // Ugh! -- this is SVG's weird default line join
        _edge->AttachEdge(&_vin, &v1);
        _edge->AttachEdge(&v2, &_vout);
        if (xprod < 0)
            _edge->AttachEdge(&v4, &v2);
        else
            _edge->AttachEdge(&v1, &v3);

        _vin = v3;
        _vout = v4;
        return;  // the dirty deed is done
    }

    // LINEJOIN_MITER: This miter join will be properly clipped
    VERT16 am, vm, vz = { 0, 0 };
    XY unused;
    FIX16 dotprod = Fix16Mult(ain.x, aout.x) + Fix16Mult(ain.y, aout.y);

    // Set vector vm to point from v0 to the miter point
    if (dotprod < 0)
    {
        // Incoming/outgoing lines form acute angle at join
        vm.x = ain.x - aout.x;
        vm.y = ain.y - aout.y;
    }
    else  // in/out lines form oblique angle at join
    {
        if (xprod < 0)
        {
            // Stroke turns left (CCW) at join
            vm.x = -ain.y - aout.y;
            vm.y = ain.x + aout.x;
        }
        else
        {
            // Stroke turns right (CW) at join
            vm.x = ain.y + aout.y;
            vm.y = -ain.x - aout.x;
        }
    }

    // Adjust length of vector am to miter-limited length
    LineLength(vz, vm, &unused, &am);
    am.x = Fix16Mult(am.x, _miterlimit);
    am.y = Fix16Mult(am.y, _miterlimit);

    // Extend sides of stroked lines to miter limit
    denom = abs(ain.x - aout.x) + abs(ain.y - aout.y);
    if (fabs(denom) > 0.00025f)  // verify denominator is not close to zero
    {
        if (xprod < 0)
        {
            // Stroke turns left (CCW) at join
            numer = abs(2*am.x + ain.y + aout.y) + abs(2*am.y - ain.x - aout.x);
            t = numer/denom;  // linear interpolation parameter
            v2.x += t*ain.x;
            v2.y += t*ain.y;
            v4.x -= t*aout.x;
            v4.y -= t*aout.y;
            _edge->AttachEdge(&v4, &v2);
        }
        else
        {
            // Stroke turns right (CW) at join
            numer = abs(2*am.x - ain.y - aout.y) + abs(2*am.y + ain.x + aout.x);
            t = numer/denom;  // linear interpolation parameter
            v1.x += t*ain.x;
            v1.y += t*ain.y;
            v3.x -= t*aout.x;
            v3.y -= t*aout.y;
            _edge->AttachEdge(&v1, &v3);
        }
    }
    _edge->AttachEdge(&_vin, &v1);
    _edge->AttachEdge(&v2, &_vout);
    _vin = v3;
    _vout = v4;
}

//---------------------------------------------------------------------
//
// Public function: Strokes the current path. First, the sides of the
// stroked path are offset by _linewidth/2 from the line segments
// defined in the original path. Joins are constructed to connect the
// sides of adjoining stroked segments, and any open line ends are
// capped. Then the points in the stroked path are converted to a list
// of polygonal edges.
//
//----------------------------------------------------------------------

bool PathMgr::StrokePath()
{
    // Tie up any loose ends in the final figure of the current path
    EndFigure();
    if (_figure->offset == 0)
        return false;  // path is empty

    if (_linewidth == 0)  // special case: Mimic Bresenham line
        return ThinStrokePath();

    _figtmp = _figure;  // this empty figure terminates the path

    // Each while-loop iteration below processes one figure (aka
    // subpath), starting with the last figure in the path
    int off;
    while ((off = _figtmp->offset) != 0)
    {
        bool dashon0 = InitLineDash();  // initial _dashon state
        VERT16 *vs0, a0, vin0, vout0;  // save initial values
        XY uin, uout, u0;  // unit vector in line direction
        VERT16 ain, aout;  // vectors of length = _linewidth/2
        FIX16 linelen;     // length of current line segment
        VERT16 *fpt = reinterpret_cast<VERT16*>(_figtmp + 1);
        VERT16 *vs = &fpt[-off];  // start of 1st line segment
        VERT16 *ve = &vs[1];    // end of 1st line segment
        int nlines = off - 2;  // number of line segments

        assert(vs != ve);  // figure has at least 2 points
        _figtmp = &_figtmp[-off];  // point to header for new figure

        vs0 = vs;
        linelen = LineLength(*vs, *ve, &uin, &ain);
        a0 = ain;
        u0 = uin;

        // Offset stroked edges _linewidth/2 from initial line segment
        if (dashon0)
        {
            _vin = _vout = *vs;
            _vin.x += ain.y;
            _vin.y -= ain.x;
            _vout.x -= ain.y;
            _vout.y += ain.x;
            vin0 = _vin;
            vout0 = _vout;
        }
        if (_pdash != 0)
            DashedLine(*ve, uin, ain, linelen);

        // Each while-loop constructs a line segment and join
        vs = ve++;
        while (--nlines > 0)
        {
            linelen = LineLength(*vs, *ve, &uout, &aout);
            if (_dashon)
            {
                if (_linejoin == LINEJOIN_ROUND)
                    _angle = GetAngle(uin, uout);

                JoinLines(*vs, ain, aout);
            }

            if (_pdash != 0)
                DashedLine(*ve, uout, aout, linelen);

            uin = uout;
            ain = aout;
            vs = ve++;
        }

        // Finish off two ends of figure by capping or joining them
        if (_figtmp->isclosed && dashon0 && _dashon)
        {
            // Join the two ends of the figure together
            if (_linejoin == LINEJOIN_ROUND)
                _angle = GetAngle(uin, u0);

            JoinLines(*vs0, ain, a0);
            _edge->AttachEdge(&_vin, &vin0);
            _edge->AttachEdge(&vout0, &_vout);
        }
        else
        {
            VERT16 v1, v2;

            if (_dashon)
            {
                // Cap final point in figure
                v1 = *vs;
                if (_lineend == LINEEND_SQUARE)
                {
                    v1.x += ain.x;
                    v1.y += ain.y;
                }
                v2 = v1;
                v1.x += ain.y;
                v1.y -= ain.x;
                v2.x -= ain.y;
                v2.y += ain.x;
                _edge->AttachEdge(&_vin, &v1);
                _edge->AttachEdge(&v2, &_vout);
                if (_lineend == LINEEND_ROUND)
                {
                    VERT16 a2 = ain;

                    FlipVector(a2);
                    _angle = FIX_PI;
                    RoundJoin(*vs, ain, a2);
                }
                else
                    _edge->AttachEdge(&v1, &v2);  // square or butt cap
            }
            if (dashon0)
            {
                // Cap first point in figure
                if (_lineend != LINEEND_ROUND)
                {
                    if (_lineend == LINEEND_SQUARE)
                    {
                        v1 = vin0;
                        v2 = vout0;
                        vin0.x -= a0.x;
                        vin0.y -= a0.y;
                        vout0.x -= a0.x;
                        vout0.y -= a0.y;
                        _edge->AttachEdge(&vin0, &v1);
                        _edge->AttachEdge(&v2, &vout0);
                    }
                    _edge->AttachEdge(&vout0, &vin0);  // square or butt cap
                }
                else
                {
                    VERT16 a1 = a0;

                    FlipVector(a1);
                    _angle = FIX_PI;
                    RoundJoin(*vs0, a1, a0);
                }
            }
        }
    }
    _figtmp = 0;

    // Fill within the polygonal boundaries of the stroked path
    _edge->TranslateEdges(_devicecliprect.x, _devicecliprect.y);
    _edge->NormalizeEdges(FILLRULE_WINDING);
    _edge->ClipEdges(FILLRULE_INTERSECT);
    return _edge->FillEdgeList();
}

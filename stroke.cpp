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
// stroke.cpp:
//   Path manager member functions for constructing stroked paths
//
//---------------------------------------------------------------------

#include <math.h>
#include "shapepri.h"

//---------------------------------------------------------------------
//
// Public function: Sets the stroked line width. Parameter width
// specifies the line width in pixels. The default line width is
// 4.0 pixels.
//
//----------------------------------------------------------------------

float PathMgr::SetLineWidth(float width)
{
    float oldwidth = _linewidth/65536.0;

    assert(width >= 0);
    _linewidth = 65536.0*width;  // convert to 16.16 fixed-point format
    return oldwidth;
}

//---------------------------------------------------------------------
//
// Public function: Sets the line end (aka cap) style for stroked
// lines. Parameter capstyle is restricted to the values LINEEND_FLAT
// (0), LINEEND_ROUND (1), and LINEEND_SQUARE (2). If capstyle is set
// to an illegal value, the function faults. The default line end style
// is LINEEND_FLAT.
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
    case LINEJOIN_MITER:
    case LINEJOIN_ROUND:
    case LINEJOIN_BEVEL:
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
// join. Parameter mlim should be a value in the range 1.0 to 100.0;
// otherwise, the function clamps it to this range. A value of
// mlim = 1.0 cuts off miters at any angle to produce beveled
// joins. The default miter limit value is 10.0.
//
//---------------------------------------------------------------------

float PathMgr::SetMiterLimit(float mlim)
{
    float oldmlim = _miterlimit/65536.0;

    mlim = max(mlim, MITERLIMIT_MINIMUM);
    mlim = min(mlim, MITERLIMIT_MAXIMUM);
    _miterlimit = 65536.0*mlim;
    _mitercheck = 65536.0*sqrt(mlim*mlim - 1.0);
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
// the dash array is 15 elements, not counting the terminating zero.
// The default line pattern is a solid line. This default can always
// be restored by calling SetLineDash with the dash parameter set to
// zero or pointing to an empty string.
//
//---------------------------------------------------------------------

void PathMgr::SetLineDash(char *dash, int offset, float mult)
{
    memset(_dasharray, 0, sizeof(_dasharray));
    if (dash == 0 || dash[0] == '\0')
        return;  // solid line -- no dash pattern

    for (int i = 0; i < ARRAY_LEN(_dasharray) && dash[i] != '\0'; ++i)
    {
        _dasharray[i] = 65536.0*mult*static_cast<unsigned char>(dash[i]);
        assert(_dasharray[i] < (1 << 30));
    }
    _dasharray[ARRAY_LEN(_dasharray)-1] = 0;  // force terminating zero
    _dashoffset = 65536.0*mult*offset;
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
// receives a unit vector that points in the direction of the line.
// Output parameter a receives a vector of length _linewidth/2 that
// points in the same direction as u. The return value is the length
// (in pixels) of the line segment.
//
//----------------------------------------------------------------------

FIX16 PathMgr::LineLength(const VERT16& vs, const VERT16& ve, VERT30 *u, VERT16 *a)
{
    FIX16 length;

    a->x = ve.x - vs.x;
    a->y = ve.y - vs.y;
    assert((a->x | a->y) != 0);

    length = fixlen(a->x, a->y);
    u->x = fixdiv(a->x, length, 30);  // unit vector, 30-bit fraction
    u->y = fixdiv(a->y, length, 30);

    a->x = fixmpy(u->x, _linewidth/2, 30);  // vector, 16-bit fraction
    a->y = fixmpy(u->y, _linewidth/2, 30);
    return length;
}

//----------------------------------------------------------------------
//
// Private function: Adds a round cap to the start or end point of a
// stroked line segment. Parameter vert is the start or end point of the
// line segment. Parameter a is a vector of length _linewidth/2 that
// points in the direction of the line segment. Parameter asign
// indicates whether vert is the start or end point of the directed
// line segment. If asign < 0, vert is the start point; otherwise,
// vert is the end point.
//
//----------------------------------------------------------------------

void PathMgr::RoundCap(const VERT16& vert, VERT16 a, int asign)
{
    VERT16 *p, *q, v1, v2;
    int count;

    // Path should be properly terminated with empty figure
    assert(_cpoint == 0);
    assert(_fpoint == reinterpret_cast<VERT16*>(_figure + 1));
    assert(_figure->isclosed == false);

    // Which end of the directed line segment is to be capped?
    if (asign < 0)
    {
        a.x = -a.x;  // vert is the start point
        a.y = -a.y;
    }

    // Set center-relative start/end coordinates for half circle
    v1.x =  a.y;
    v1.y = -a.x;
    v2.x = -a.y;
    v2.y =  a.x;

    // Set up temporary buffer on path stack for cap segments
    _cpoint = _fpoint;
    _cpoint->x = vert.x + a.x;
    _cpoint->y = vert.y + a.y;

    // Form a half circle from two symmetrical quarter circles.
    // Note that the EllipseCore function call updates _cpoint.
    EllipseCore(vert.x, vert.y, a.x, a.y, v1.x, v1.y, FIX_PI/2);
    PathCheck(++_cpoint);
    _cpoint->x = v1.x + vert.x;
    _cpoint->y = v1.y + vert.y;
    for (p = _fpoint, q = _cpoint; p < q; ++p, --q)
    {
        VERT16 swap = *p;  *p = *q;  *q = swap;
    }
    EllipseCore(vert.x, vert.y, a.x, a.y, v2.x, v2.y, FIX_PI/2);
    PathCheck(++_cpoint);
    _cpoint->x = v2.x + vert.x;
    _cpoint->y = v2.y + vert.y;

    // Convert points in path to polygonal edges
    p = q = _fpoint;
    count = _cpoint - _fpoint;
    for (int i = 0; i < count; ++i)
        _edge->AttachEdge(p++, ++q);

    _cpoint = 0;  // restore state of terminating figure back to empty
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

void PathMgr::DashedLine(const VERT16& ve, const VERT30& u, const VERT16& a, FIX16 linelen)
{
    assert(_pdash != 0);
    while (_dashlen <= linelen)
    {
        VERT16 vs, v1, v2;
        FIX16 dx, dy;

        // Calculate displacement to start of next dash or gap
        linelen -= _dashlen;
        dx = fixmpy(linelen, u.x, 30);
        dy = fixmpy(linelen, u.y, 30);
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
                RoundCap(vs, a, 1);
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
                RoundCap(vs, a, -1);
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

    // Check for round line join
    if (_linejoin == LINEJOIN_ROUND)
    {
        // Construct the line segment that enters the line join
        _edge->AttachEdge(&_vin, &v1);
        _edge->AttachEdge(&v2, &_vout);

        // Construct the line join (three implementations)
#if 1
        RoundCap(v0, ain, 1);  // draw arc from v1 to v2
        _edge->AttachEdge(&v4, &v3);
#elif 0
        RoundCap(v0, aout, -1);  // draw arc from v4 to v3
        _edge->AttachEdge(&v1, &v2);
#else
        RoundCap(v0, ain, 1);  // draw arc from v1 to v2
        RoundCap(v0, aout, -1);  // draw arc from v4 to v3
#endif
        _vin = v3;
        _vout = v4;
        return;
    }

    // Connect the edges on the "inside" of the bend that's formed
    // where the incoming and outgoing lines meet to form the joint
    FIX16 xprod = fixmpy(ain.x, aout.y, 16) - fixmpy(ain.y, aout.x, 16);
    if (xprod < 0)
    {   
        // Stroke turns left (CCW) at joint
        _edge->AttachEdge(&v1, &v0);
        _edge->AttachEdge(&v0, &v3);
    }
    else
    {   
        // Stroke turns right (CW) at joint
        _edge->AttachEdge(&v4, &v0);
        _edge->AttachEdge(&v0, &v2);
    }

    // Check for beveled line join
    if (_linejoin == LINEJOIN_BEVEL)
    {
        // Construct the line segment that enters the line join
        _edge->AttachEdge(&_vin, &v1);
        _edge->AttachEdge(&v2, &_vout);

        // Add the bevel
        if (xprod < 0)
        {   
            // Stroke turns left (CCW) at joint
            _edge->AttachEdge(&v4, &v2);
        }
        else
        {   
            // Stroke turns right (CW) at joint
            _edge->AttachEdge(&v1, &v3);
        }
        _vin = v3;
        _vout = v4;
        return;
    } 

    // This is a miter joint. Determine whether the miter length
    // exceeds the specified miter limit.
    FIX16 t;
    FIX16 numer = abs(ain.x - aout.x) + abs(ain.y - aout.y);
    FIX16 denom = abs(ain.x + aout.x) + abs(ain.y + aout.y);

    if (abs(denom) > 15)  // verify denominator is not close to zero
    {   
        t = fixdiv(numer, denom, 16);  // linear interpolation param
        if (t <= _mitercheck)
        {
            FIX16 dx = fixmpy(t, ain.x - aout.x, 16);
            FIX16 dy = fixmpy(t, ain.y - aout.y, 16);

            // Miter length is within miter limit, so draw full miter
            if (xprod < 0)
            {   
                // Stroke turns left (CCW) at joint
                v4.x = (v2.x + v4.x + dx)/2;
                v4.y = (v2.y + v4.y + dy)/2;
                _edge->AttachEdge(&_vin, &v1);
                _edge->AttachEdge(&v4, &_vout);
            }
            else
            {   
                // Stroke turns right (CW) at joint
                v3.x = (v1.x + v3.x + dx)/2;
                v3.y = (v1.y + v3.y + dy)/2;
                _edge->AttachEdge(&_vin, &v3);
                _edge->AttachEdge(&v2, &_vout);
            }
            _vin = v3;
            _vout = v4;
            return;
        }
    }

    // Miter limit exceeded: we'll have to snip off miter point
    VERT16 am, vm, vz = { 0, 0 };
    VERT30 unused;
    FIX16 dotprod = fixmpy(ain.x, aout.x, 16) + fixmpy(ain.y, aout.y, 16);

    // Set vector vm to point from v0 to the miter point
    if (dotprod < 0)  
    {   
        // Incoming/outgoing lines form acute angle at joint
        vm.x = ain.x - aout.x;
        vm.y = ain.y - aout.y;
    }
    else  // in/out lines form oblique angle at joint
    {
        if (xprod < 0)  
        {   
            // Stroke turns left (CCW) at joint
            vm.x = -ain.y - aout.y;
            vm.y = ain.x + aout.x;
        }
        else  
        {   
            // Stroke turns right (CW) at joint
            vm.x = ain.y + aout.y;
            vm.y = -ain.x - aout.x;
        }
    }

    // Adjust length of vector am to miter-limited length
    LineLength(vz, vm, &unused, &am);
    am.x = fixmpy(am.x, _miterlimit, 16);
    am.y = fixmpy(am.y, _miterlimit, 16);

    // Extend sides of stroked lines to miter limit
    denom = abs(ain.x - aout.x) + abs(ain.y - aout.y);
    if (abs(denom) > 15)  // verify denominator is not close to zero
    {
        if (xprod < 0)
        {
            // Stroke turns left (CCW) at joint
            numer = abs(2*am.x + ain.y + aout.y) + abs(2*am.y - ain.x - aout.x);
            t = fixdiv(numer, denom, 16);  // linear interpolation param
            v2.x += fixmpy(t, ain.x, 16);
            v2.y += fixmpy(t, ain.y, 16);
            v4.x -= fixmpy(t, aout.x, 16);
            v4.y -= fixmpy(t, aout.y, 16);
            _edge->AttachEdge(&v4, &v2);
        }
        else
        {
            // Stroke turns right (CW) at joint
            numer = abs(2*am.x - ain.y - aout.y) + abs(2*am.y + ain.x + aout.x);
            t = fixdiv(numer, denom, 16);  // linear interpolation param
            v1.x += fixmpy(t, ain.x, 16);
            v1.y += fixmpy(t, ain.y, 16);
            v3.x -= fixmpy(t, aout.x, 16);
            v3.y -= fixmpy(t, aout.y, 16);
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
        bool dashon0 = InitLineDash();   // initial _dashon state
        VERT16 *vs0, a0, vin0, vout0;    // to save initial values
        VERT30 uin, uout;  // unit vectors in directions of lines  
        VERT16 ain, aout;  // vectors of length = _linewidth/2
        FIX16 linelen;     // length of current line segment
        VERT16 *fpt = reinterpret_cast<VERT16*>(_figtmp + 1);
        VERT16 *vs = &fpt[-2];    // last point in new figure
        VERT16 *ve = &fpt[-off];  // first point in new figure
        int nlines = off - 2;

        assert(vs != ve);  // figure has at least 2 points
        _figtmp = &_figtmp[-off];  // point to header for new figure
        if (_figtmp->isclosed)
            ++nlines;  // add extra line to connect start/end points
        else
            vs = ve++;  // don't connect start/end points

        vs0 = vs;
        linelen = LineLength(*vs, *ve, &uin, &ain);
        a0 = ain;
        
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
                JoinLines(*vs, ain, aout);

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
                    RoundCap(*vs, ain, 1);
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
                    RoundCap(*vs0, a0, -1);
            }
        }
    }
    _figtmp = 0;

    // Fill within the polygonal boundaries of the stroked path
    _edge->TranslateEdges(_scroll.x, _scroll.y);
    _edge->NormalizeEdges(FILLRULE_WINDING);
    _edge->ClipEdges(FILLRULE_INTERSECT);
    return _edge->FillEdgeList();
}

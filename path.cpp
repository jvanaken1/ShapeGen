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
// path.cpp:
//   Path manager member functions for setting path parameters, and
//   for constructing paths that can be converted to polygonal edge
//   lists and filled by a renderer
//
//---------------------------------------------------------------------

#include <math.h>
#include "shapepri.h"

//---------------------------------------------------------------------
//
// Creates a ShapeGen object and returns a pointer to this object
//
//---------------------------------------------------------------------

ShapeGen* CreateShapeGen(Renderer *renderer, const SGRect& cliprect)
{
    PathMgr *sg = new PathMgr(renderer, cliprect);
    if (sg == 0 || sg->GetStatus() == false)
    {
        assert(sg != 0 && sg->GetStatus() == true);
        delete sg;
        return 0;  // constructor failed
    }
    return sg;  // success
}

//---------------------------------------------------------------------
//
// PathMgr constructor and destructor (both protected)
//
//---------------------------------------------------------------------

PathMgr::PathMgr(Renderer *renderer, const SGRect& cliprect) :
            _path(0), _edge(0), _pathlength(INITIAL_PATH_LENGTH),
            _angle(0), _fpoint(0), _cpoint(0), _figure(0), _figtmp(0),
            _dashoffset(0), _pdash(0), _dashlen(0), _dashon(true),
            _devicecliprect(cliprect), _fixshift(16),
            _flatness(FLATNESS_DEFAULT), _fillrule(FILLRULE_DEFAULT),
            _linewidth(LINEWIDTH_DEFAULT), _lineend(LINEEND_DEFAULT),
            _linejoin(LINEJOIN_DEFAULT), _miterlimit(MITERLIMIT_DEFAULT)
{
    assert(sizeof(VERT16) == sizeof(FIGURE));  // for path stack
    if (renderer == 0)
    {
        assert(renderer != 0);
        return;  // fail - invalid parameter value
    }
    if (cliprect.w < 1 || cliprect.h < 1)
    {
        assert(cliprect.w > 0 && cliprect.h > 0);
        return;  // fail - invalid parameter value
    }
    _edge = new EdgeMgr();
    if (_edge == 0)
    {
        assert(_edge != 0);
        return;  // fail - out of memory
    }
    _path = new VERT16[_pathlength];
    if (_path == 0)
    {
        assert(_path != 0);
        delete _edge;
        _edge = 0;
        return;  // fail - out of memory
    }
    SetRenderer(renderer);
    InitClipRegion(cliprect.w, cliprect.h);
    SetFixedBits(0);
    SetFlatness(FLATNESS_DEFAULT);
    SetFillRule(FILLRULE_DEFAULT);
    SetLineWidth(LINEWIDTH_DEFAULT);
    SetLineEnd(LINEEND_DEFAULT);
    SetLineJoin(LINEJOIN_DEFAULT);
    SetMiterLimit(MITERLIMIT_DEFAULT);
    SetLineDash(0, 0, 0);
    BeginPath();
}

PathMgr::~PathMgr()
{
    delete _edge;
    delete[] _path;
}

bool PathMgr::GetStatus()
{
    return (_path != 0);  // did constructor succeed?
}

//---------------------------------------------------------------------
//
// Public function: Sets the Renderer object that the ShapeGen object
// will use to draw filled shapes on the display
//
//---------------------------------------------------------------------

bool PathMgr::SetRenderer(Renderer *renderer)
{
    if (_edge->SetRenderer(renderer) == false)
        return false;

    ResetClipRegion();  // N.B.: new renderer can change y resolution
    renderer->SetMaxWidth(_devicecliprect.w);
    renderer->SetScrollPosition(_devicecliprect.x, _devicecliprect.y);
    _renderer = renderer;
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Sets the scroll position for the display window.
// Parameters x and y specify the x-y displacements to subtract from
// the points in any path before they are drawn. This feature permits
// the viewer to scroll and pan through a constructed image that is
// larger than the display window.
//
//---------------------------------------------------------------------

void PathMgr::SetScrollPosition(int x, int y)
{
    _devicecliprect.x = x;
    _devicecliprect.y = y;
    _renderer->SetScrollPosition(x, y);  // to scroll fill patterns
}

//---------------------------------------------------------------------
//
// Private function: Grows the size of the path memory in the event
// of a path stack overflow. Allocates a larger stack and copies the
// contents of the old stack to the new stack. The path management
// functions are designed so that the only pointers that need to be
// updated are _cpoint, _fpoint, _figure, and _figtmp. The path data
// itself contains no pointers, only path-relative offsets.
//
//---------------------------------------------------------------------

void PathMgr::GrowPath()
{
    if (_path == 0)
    {
        assert(_path != 0);
        return;  // fail - out of memory
    }
    int offset, oldlen = _pathlength;
    VERT16 *oldpath = _path;

    _pathlength += _pathlength;
    _path = new VERT16[_pathlength];

    // TODO: Replace assert below with out-of-memory exception
    assert(_path != 0);
    memcpy(_path, oldpath, oldlen*sizeof(_path[0]));
    if (_cpoint != 0)
    {
        offset = _cpoint - oldpath;
        _cpoint = &_path[offset];
    }
    if (_fpoint != 0)
    {
        offset = _fpoint - oldpath;
        _fpoint = &_path[offset];
    }
    if (_figure != 0)
    {
        offset = _figure - reinterpret_cast<FIGURE*>(oldpath);
        _figure = reinterpret_cast<FIGURE*>(&_path[offset]);
    }
    if (_figtmp != 0)
    {
        offset = _figtmp - reinterpret_cast<FIGURE*>(oldpath);
        _figtmp = reinterpret_cast<FIGURE*>(&_path[offset]);
    }
    delete[] oldpath;
}

//---------------------------------------------------------------------
//
// Public function: Specifies the minimum flatness (curve-to-chord
// error tolerance) required of a curve segment before it can be
// satisfactorily replaced by a chord (straight line segment).
// Parameter tol specifies the required flatness and is restricted to
// the range 0.2 to 100 pixels.
//
//----------------------------------------------------------------------

float PathMgr::SetFlatness(float tol)
{
    float oldtol = _flatness/65536.0f;

    tol = max(tol, FLATNESS_MINIMUM);
    tol = min(tol, FLATNESS_MAXIMUM);
    _flatness = 65536*tol;  // convert to 16.16 fixed-point format
    return oldtol;
}

//---------------------------------------------------------------------
//
// Public function: Specifies the fixed-point representation to use for
// the x-y coordinates that the user supplies to ShapeGen interface
// functions. Parameter nbits is the number of bits to the right of the
// fixed point. By default, user-supplied coordinates are assumed to be
// integers (that is, nbits = 0). Internally, the path manager uses
// 16.16 fixed-point values, corresponding to nbits = 16. Parameter
// nbits must be in the range 0 to 16 or the function faults.
//
//----------------------------------------------------------------------

int PathMgr::SetFixedBits(int nbits)
{
    int oldnbits = 16 - _fixshift;

    if (nbits < 0 || 16 < nbits)
    {
        assert(0 <= nbits && nbits <= 16);
        return -1;
    }
    _fixshift = 16 - nbits;
    return oldnbits;
}

//---------------------------------------------------------------------
//
// Public function: Begins a new path at the start of the allocated
// path memory. Sets up the first figure in this path, which is
// initially empty.
//
//----------------------------------------------------------------------

void PathMgr::BeginPath()
{
    _figure = reinterpret_cast<FIGURE*>(&_path[0]);
    _figure->offset = 0;
    _figure->isclosed = false;  // by default, path is open
    _fpoint = &_path[1];
    _cpoint = 0;  // indicate figure is empty
}

//---------------------------------------------------------------------
//
// Private function: Finalizes the current figure, and then opens a
// new, initially empty figure. If a figure contains just one point,
// that point is discarded. Input parameter bclose is true if the
// figure is closed; otherwise (i.e., the figure is open), it's false.
//
//----------------------------------------------------------------------

void PathMgr::FinalizeFigure(bool bclose)
{
    if (_cpoint != 0)
    {
        if (_cpoint != _fpoint)  // more than one point?
        {
            int count = _cpoint - _fpoint;
            VERT16 *p = _fpoint, *q = p;

            // Before starting a new figure, remove any
            // redundant points from the current figure
            for (int i = 0; i < count; ++i)
            {
                ++q;
                if ((p->x != q->x || p->y != q->y) && ++p != q)
                    *p = *q;
            }

            if (p != _fpoint)
            {
                if (bclose)
                {
                    _figure->isclosed = true;
                    if (p->x != _fpoint->x || p->y != _fpoint->y)
                        *++p = *_fpoint;  // handy for stroked paths
                }
                _cpoint = p;

                // Start a new figure in the same path
                PathCheck(++_cpoint);
                _figure = reinterpret_cast<FIGURE*>(_cpoint);
                _figure->isclosed = false;
                PathCheck(++_cpoint);
                _figure->offset = _cpoint - _fpoint;
                _fpoint = _cpoint;
            }
        }
        _cpoint = 0;  // empty figure
    }
}
//---------------------------------------------------------------------
//
// Public function: Closes the current figure (aka subpath) by adding a
// line segment to connect the current point to the first point in the
// figure. Closing a figure terminates the figure and begins a new,
// empty figure in the same path. If the current figure is empty (that
// is, its first point is undefined), a CloseFigure call has no effect.
// If the current figure contains only a single point, the current
// figure is replaced by a new, initially empty figure.
//
//----------------------------------------------------------------------

void PathMgr::CloseFigure()
{
    FinalizeFigure(true);
}

//----------------------------------------------------------------------
//
// Public function: Ties up any loose ends in the current figure (aka
// subpath) in the current path. The function then starts a new figure
// in the same path. The new figure is initially empty. In contrast to
// the CloseFigure function, the EndFigure function terminates the
// current figure without adding a line segment to connect the start
// and end points of the figure. If the current figure is empty, the
// EndFigure call has no effect. If the current figure contains only a
// single point, the current figure is replaced with a new, initially
// empty figure.
//
//----------------------------------------------------------------------

void PathMgr::EndFigure()
{
    FinalizeFigure(false);
}

//---------------------------------------------------------------------
//
// Public function: Moves the current point to a new position. This
// call terminates the current figure (equivalent to an EndFigure
// call), and automatically starts a new figure in the same path. This
// new figure contains a single point, whose x-y coordinates are
// specified by parameters x and y.
//
//---------------------------------------------------------------------

void PathMgr::Move(SGCoord x, SGCoord y)
{
    EndFigure();
    _cpoint = _fpoint;
    _cpoint->x = x << _fixshift;
    _cpoint->y = y << _fixshift;
}

//---------------------------------------------------------------------
//
// Public function: Appends a line segment from the current point to
// the point specified by parameters x and y. If the current figure is
// empty, the function faults.
//
//----------------------------------------------------------------------

bool PathMgr::Line(SGCoord x, SGCoord y)
{
    if (_cpoint == 0)
    {
        assert(_cpoint != 0);
        return false;
    }
    PathCheck(++_cpoint);
    _cpoint->x = x << _fixshift;
    _cpoint->y = y << _fixshift;
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Appends a series of connected line segments to the
// current figure. Parameter xy is an array that contains the points in
// the polyline. Parameter npts specifies the number of points in this
// array. The current point serves as the starting point for the
// polyline, and npts line segments specified in the xy array are
// appended to the path.
//
//----------------------------------------------------------------------

bool PathMgr::PolyLine(const SGPoint xy[], int npts)
{
    if (_cpoint == 0 || npts < 0 || xy == 0)
    {
        assert(_cpoint != 0 && npts >= 0 && xy != 0);
        return false;
    }
    for (int i = 0; i < npts; ++i)
    {
        PathCheck(++_cpoint);
        _cpoint->x = xy[i].x << _fixshift;
        _cpoint->y = xy[i].y << _fixshift;
    }
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Appends a rectangle to the current path. First, the
// function terminates the current figure (equivalent to an EndFigure
// call), if it is not empty. Then it adds the rectangle to the path
// as a closed figure. Rotation direction = clockwise.
//
//----------------------------------------------------------------------

void PathMgr::Rectangle(const SGRect& rect)
{
    SGCoord x, y;

    x = rect.x;
    y = rect.y;
    Move(x, y);
    x += rect.w;
    Line(x, y);
    y += rect.h;
    Line(x, y);
    x -= rect.w;
    Line(x, y);
    CloseFigure();
}

//----------------------------------------------------------------------
//
// Private function: Converts the points in the current path to a list
// of polygonal edges. To prepare for a fill operation, this function
// always closes the figure by adding a line segment to connect the
// figure's start and end points.
//
//----------------------------------------------------------------------

bool PathMgr::FilledShape()
{
    // Tie up any loose ends in final figure of current path
    EndFigure();
    if (_figure->offset == 0)
        return false;  // path is empty

    // Convert points in path stack to linked list of edges for
    // polygon. Each iteration processes one figure (or subpath).
    // Always force a filled figure to be closed by connecting
    // its start and end points.
    FIGURE *fig = _figure;  // empty figure terminates path
    VERT16 *fpt = _fpoint;
    int off;

    while ((off = fig->offset) != 0)
    {
        VERT16 *vs = &fpt[-2];    // last point in new figure
        VERT16 *ve = &fpt[-off];  // first point in new figure
        int nverts = off - 1;     // number of vertices

        fpt = ve;          // remember first point
        fig = &fig[-off];  // header for new figure
        if (fig->isclosed)
        {
            --vs;
            --nverts;
        }
        if (vs != ve)
        {
            for (int i = 0; i < nverts; i++)
            {
                _edge->AttachEdge(vs, ve);
                vs = ve++;
            }
        }
    }
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Sets the fill rule to use for filling paths
//
//----------------------------------------------------------------------

FILLRULE PathMgr::SetFillRule(FILLRULE fillrule)
{
    FILLRULE oldrule = _fillrule;
    switch (fillrule)
    {
    case FILLRULE_EVENODD:
    case FILLRULE_WINDING:
        _fillrule = fillrule;
        break;
    default:
        assert(0);
        break;
    }
    return oldrule;
}

//---------------------------------------------------------------------
//
// Public function: Fills the current path
//
//----------------------------------------------------------------------

bool PathMgr::FillPath()
{
    if (FilledShape() == false)
        return false;  // path is empty

    if ((_devicecliprect.x | _devicecliprect.y) != 0)
        _edge->TranslateEdges(_devicecliprect.x, _devicecliprect.y);

    _edge->NormalizeEdges(_fillrule);
    _edge->ClipEdges(FILLRULE_INTERSECT);
    return _edge->FillEdgeList();
}

//---------------------------------------------------------------------
//
// Public function: Strokes the current path
//
//----------------------------------------------------------------------

bool PathMgr::StrokePath()
{
    if (StrokedShape() == false)
        return false;  // path is empty

    // Fill within the polygonal boundaries of the stroked path
    if ((_devicecliprect.x | _devicecliprect.y) != 0)
        _edge->TranslateEdges(_devicecliprect.x, _devicecliprect.y);

    _edge->NormalizeEdges(FILLRULE_WINDING);
    _edge->ClipEdges(FILLRULE_INTERSECT);
    return _edge->FillEdgeList();
}

//---------------------------------------------------------------------
//
// Public function: Sets the new clipping region to the intersection
// of the current clipping region and the _interior_ of the current
// path. Returns true if the new clipping region is not empty;
// otherwise, returns false.
//
//---------------------------------------------------------------------

bool PathMgr::SetClipRegion(CLIPMODE clipmode)
{
    FILLRULE rule;
    switch (clipmode)
    {
    case CLIPMODE_FILLPATH:
        rule = _fillrule;
        if (FilledShape() == false)
            return false;  // path is empty
        break;
    case CLIPMODE_STROKEPATH:
        rule = FILLRULE_WINDING;
        if (StrokedShape() == false)
            return false;  // path is empty
        break;
    default:
        assert(0);
        return false;
    }

    if ((_devicecliprect.x | _devicecliprect.y) != 0)
        _edge->TranslateEdges(_devicecliprect.x, _devicecliprect.y);

    _edge->NormalizeEdges(rule);
    _edge->ClipEdges(FILLRULE_INTERSECT);
    return _edge->SetClipList();
}

//---------------------------------------------------------------------
//
// Public function: Sets the new clipping region to the intersection
// of the current clipping region and the _exterior_ of the current
// path. Returns true if the new clipping region is not empty;
// otherwise, returns false.
//
//---------------------------------------------------------------------

bool PathMgr::SetMaskRegion(CLIPMODE clipmode)
{
    FILLRULE rule;
    switch (clipmode)
    {
    case CLIPMODE_FILLPATH:
        rule = _fillrule;
        if (FilledShape() == false)
            return false;  // path is empty
        break;
    case CLIPMODE_STROKEPATH:
        rule = FILLRULE_WINDING;
        if (StrokedShape() == false)
            return false;  // path is empty
        break;
    default:
        assert(0);
        return false;
    }

    if ((_devicecliprect.x | _devicecliprect.y) != 0)
        _edge->TranslateEdges(_devicecliprect.x, _devicecliprect.y);

    _edge->NormalizeEdges(rule);
    _edge->ReverseEdges();
    _edge->ClipEdges(FILLRULE_EXCLUDE);
    return _edge->SetClipList();
}

//---------------------------------------------------------------------
//
// Public function: Resets the clipping region to its default setting,
// which is the current device clipping rectangle
//
//---------------------------------------------------------------------

void PathMgr::ResetClipRegion()
{
    _edge->SetDeviceClipRectangle(_devicecliprect.w, _devicecliprect.h, true);
}

//---------------------------------------------------------------------
//
// Public function: Initializes the clipping region to the device
// clipping rectangle specified (in pixels) by the width and height
// parameters
//
//---------------------------------------------------------------------

bool PathMgr::InitClipRegion(int width, int height)
{
    if (width < 1 || height < 1)
    {
        assert(width > 0 && height > 0);
        return false;
    }
    _devicecliprect.w = width;
    _devicecliprect.h = height;
    _edge->SetDeviceClipRectangle(width, height, false);
    _renderer->SetMaxWidth(width);
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Retrieves the current point. If the current point
// is defined (that is, if the current figure is not empty), this
// function writes the x-y coordinates of the current point to the
// location pointed to by parameter cpoint, and returns a value of
// true. If the current point is undefined, the function immediately
// returns false. By default, coordinates are integer values, but the
// user can switch to fixed-point coordinates by calling SetFixedBits.
//
//---------------------------------------------------------------------

bool PathMgr::GetCurrentPoint(SGPoint *cpoint)
{
    if (_cpoint == 0)
        return false;  // current point is undefined

    if (cpoint != 0)
    {
        FIX16 roundoff = (_fixshift == 0) ? 0 : (1 << (_fixshift - 1));

        cpoint->x = (_cpoint->x + roundoff) >> _fixshift;
        cpoint->y = (_cpoint->y + roundoff) >> _fixshift;
    }
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Retrieves the first point in the current figure.
// If the first point is defined (that is, if the current figure is
// not empty), this function writes the x-y coordinates of the first
// point to the location pointed to by parameter fpoint, and returns
// a value of true. If the first point is undefined, the function
// immediately returns false. By default, coordinates are integer
// values, but the user can switch to fixed-point coordinates by
// calling SetFixedBits.
//
//---------------------------------------------------------------------

bool PathMgr::GetFirstPoint(SGPoint *fpoint)
{
    if (_cpoint == 0)
        return false;  // current point is undefined

    if (fpoint != 0)
    {
        FIX16 roundoff = (_fixshift == 0) ? 0 : (1 << (_fixshift - 1));

        fpoint->x = (_fpoint->x + roundoff) >> _fixshift;
        fpoint->y = (_fpoint->y + roundoff) >> _fixshift;
    }
    return true;
}

//---------------------------------------------------------------------
//
// Public function: Retrieves the minimum bounding box for the current
// path. If the path is not empty, the function writes the bounding
// box coordinates, width, and height to the structure pointed to by
// 'bbox', and then returns a count of the number of points in the
// path. If the path is empty, the function immediately returns zero
// without writing to the 'bbox' structure. The 'flags' parameter
// allows the caller to modify the function's behavior; for example,
// one flag bit instructs the function to calculate what the bounding
// box would be if the path were stroked with the current stroke
// attributes. This function does not alter the path in any way. By
// default, user coordinates are integer values, but the user can call
// the SetFixedBits function to switch to using fixed-point coordi-
// nates, in which case the values written to 'bbox' are fixed-point.
// The code below uses ShapeGen-internal coordinates, which are in
// 16.16 fixed-point format. Also, the function calculates the
// bounding box based on the x-y coordinates in the path, and does not
// actually construct any shapes.
//
//---------------------------------------------------------------------

int PathMgr::GetBoundingBox(SGRect *bbox, int flags)
{
    FIX16 xmin, ymin, xmax, ymax;
    FIGURE *fig;
    VERT16 *point;
    int offset, count = 0;

    xmin = ymin = 0x7fffffff;
    xmax = ymax = 0x80000000;
    if (_cpoint == 0)
    {
        // Current figure is empty
        offset = _figure->offset;
        if (offset == 0)
            return 0;  // current path is empty

        fig = _figure;
        point = _fpoint;
    }
    else
    {
        // Current figure is not empty, not finalized. (Note that the
        // function does not try to access the locations pointed to
        // by the initial values of 'fig' and 'point' just below, so
        // there is no risk of an unhandled path stack overflow.)
        fig = reinterpret_cast<FIGURE*>(_cpoint + 1);
        point = _cpoint + 2;
        offset = point - _fpoint;
    }
    while (offset != 0)
    {
        int npts = offset - 1;

        fig = &fig[-offset];
        point = &point[-offset];
        for (int i = 0; i < npts; ++i)
        {
            xmin = min(point[i].x, xmin);
            ymin = min(point[i].y, ymin);
            xmax = max(point[i].x, xmax);
            ymax = max(point[i].y, ymax);
        }
        count += npts;
        offset = fig->offset;
    }

    // The bbox argument can be null (zero) if the caller simply wants
    // a count of the number of points in the path
    if (!bbox)
        return count;

    // Compensate for fuzzy edges of antialiased shapes
    xmin -= 0x00008000;
    ymin -= 0x00008000;
    xmax += 0x00008000;
    ymax += 0x00008000;
    if (flags)
    {
        FIX16 xmin0, ymin0, xmax0, ymax0;
        if (flags & FLAG_BBOX_STROKE)
        {
            FIX16 pad = 0;
            if (_linewidth)
            {
                if (_linejoin == LINEJOIN_MITER || _linejoin == LINEJOIN_SVG_MITER)
                {
                   pad = sqrt(_miterlimit*_miterlimit + 1)*_linewidth/2;
                }
                else if (_lineend == LINEEND_SQUARE)
                {
                    const float sqrt2 = 1.414213562373f;
                    pad = sqrt2*_linewidth/2;
                }
                else
                    pad = _linewidth/2;
            }
            xmin -= pad;
            ymin -= pad;
            xmax += pad;
            ymax += pad;
        }
        if (flags & FLAG_BBOX_CLIP)
        {
            xmin0 = _devicecliprect.x << 16;
            ymin0 = _devicecliprect.y << 16;
            xmax0 = xmin0 + (_devicecliprect.w << 16);
            ymax0 = ymin0 + (_devicecliprect.h << 16);

            xmin = max(xmin0, xmin);
            ymin = max(ymin0, ymin);
            xmax = min(xmax0, xmax);
            ymax = min(ymax0, ymax);

            if ((xmax - xmin) < 1 || (ymax - ymin) < 1)
                return 0;  // clipped path is empty
        }
        if ((flags & FLAG_BBOX_ACCUM) && (bbox->w > 0) && (bbox->h > 0))
        {
            xmin0 = bbox->x << _fixshift;
            ymin0 = bbox->y << _fixshift;
            xmax0 = xmin0 + (bbox->w << _fixshift);
            ymax0 = ymin0 + (bbox->h << _fixshift);

            xmin = min(xmin0, xmin);
            ymin = min(ymin0, ymin);
            xmax = max(xmax0, xmax);
            ymax = max(ymax0, ymax);
        }
    }

    // Extend sides of bbox outward to next pixel boundary
    xmax += 0x0000ffff;
    ymax += 0x0000ffff;
    xmin &= 0xffff0000;
    ymin &= 0xffff0000;
    xmax &= 0xffff0000;
    ymax &= 0xffff0000;

    // Convert coordinates to user's current integer/fixed-point format
    bbox->x = xmin >> _fixshift;
    bbox->y = ymin >> _fixshift;
    bbox->w = (xmax - xmin) >> _fixshift;
    bbox->h = (ymax - ymin) >> _fixshift;
    return count;
}

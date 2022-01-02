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
// edge.cpp:
//   Polygonal edge manager: Converts paths into lists of polygonal
//   edges. The output edge list is in normalized form, which means
//   that each successive pair of edges describes the left and right
//   edges of the same trapezoid. The trapezoids in this list are 
//   non-overlapping, clipped, and ready to be filled by a renderer.
//
//---------------------------------------------------------------------

#include "shapepri.h"
#include <stdlib.h>

namespace {
    //---------------------------------------------------------------------
    //
    // A qsort comparison function used to sort a list of edges in order of
    // the y coordinates at their top (minimum-y) end. The items in this
    // list are sorted in order of their ascending ytop values.
    //
    //----------------------------------------------------------------------
    
    int ycomp(const void *key1, const void *key2)
    {
        EDGE *p = *(EDGE**)key1;
        EDGE *q = *(EDGE**)key2;
    
        return (p->ytop - q->ytop);
    }
    
    //---------------------------------------------------------------------
    //
    // A qsort comparison function used to sort a list of edges in order of
    // the x coordinates at their top (minimum-y) end. The items in this
    // list are sorted in order of their ascending xtop values. If two edges
    // have equal xtop values, these edges are sorted in order of ascending
    // dxdy values. Two edges that have matching xtop and dxdy values are
    // sorted in order of their descending dy values.
    //
    //----------------------------------------------------------------------
    
    int xcomp(const void *key1, const void *key2)
    {
        EDGE *p = *(EDGE**)key1;
        EDGE *q = *(EDGE**)key2;
    
        if (p->xtop == q->xtop)
        {
            if (p->dxdy != q->dxdy)
                return (p->dxdy - q->dxdy);
    
            return (q->dy - p->dy);  // sort coincident edges
        }
        return (p->xtop - q->xtop);
    }

    //---------------------------------------------------------------------
    //
    // Uses the qsort function in stdlib.h to sort items in a singly linked
    // EDGE list. Parameter plist points to the head of the list. Parameter
    // length is the number of items in the list. Parameter comp is the
    // comparison function. The sortlist function returns a pointer to the
    // head of the new, sorted list.
    //
    //---------------------------------------------------------------------
    
    EDGE* sortlist(EDGE *plist, int length, int (*comp)(const void *, const void *))
    {
        int i, count = 0;
        EDGE **ptr = new EDGE*[length];  // pointer array for qsort
    
        assert(ptr);
        for (EDGE *p = plist; p; p = p->next)
            ptr[count++] = p;
    
        //assert(count == length);
        qsort(ptr, count, sizeof(EDGE*), comp);   // stdlib.h function
        for (i = 1; i < count; i++)
            ptr[i-1]->next = ptr[i];  // update links in linked list
    
        ptr[i-1]->next = 0;
        EDGE *tmp = ptr[0];
        delete[] ptr;
        return tmp;
    }
}

//---------------------------------------------------------------------
//
// EDGE allocation pool functions
//
//---------------------------------------------------------------------

POOL::POOL(int len) : blklen(len), watermark(0), index(0), count(0)
{
    block = new EDGE[blklen];
    assert(block);
    memset(inventory, 0, ARRAY_LEN(inventory)*sizeof(EDGE*));
}

POOL::~POOL()
{
    delete[] block;
    for (int i = 0; i < index; ++i)
        delete[] inventory[i];
}

void POOL::AcquireBlock()
{
    // The current block is 100 percent allocated. Acquire a new block.
    assert(index != ARRAY_LEN(inventory));
    inventory[index++] = block;
    count += blklen;
    blklen += blklen;
    block = new EDGE[blklen];
    assert(block != 0);
    watermark = 0;
}

void POOL::Reset()
{
    // Reset the pool by discarding all currently allocated EDGE structures.
    // Get ready for the first new allocation request.
    if (index)
    {
        EDGE *swap = block;  block = inventory[0];  inventory[0] = swap;
        for (int i = 0; i < index; ++i)
        {
            delete[] inventory[i];
            inventory[i] = 0;
            blklen = blklen/2;
        }
        index = count = 0;
    }
    watermark = 0;
}

//---------------------------------------------------------------------
//
// Shape feeder: Breaks a shape (stored as a normalized edge list) into
// smaller pieces to feed to a renderer
//
//---------------------------------------------------------------------

class Feeder : ShapeFeeder
{
    friend EdgeMgr;

    EDGE *_list, *_edgeL, *_edgeR;
    FIX16 _xL, _xR, _dxL, _dxR;
    int _ytop, _height;
    
    EDGE* ysortlist(EDGE *plist, int length);

protected:
    Feeder() : _list(0), _edgeL(0), _edgeR(0), _ytop(0),
               _height(0), _xL(0), _xR(0), _dxL(0), _dxR(0)
    {
    }
    ~Feeder()
    {
    }
    void SetEdgeList(EDGE *list, int length)
    {
        if (length)
            _list = ysortlist(list, length/2);  // antialiasing
        else
            _edgeL = list;  // no antialiasing
    }

public:
    bool GetNextGDIRect(SGRect *rect);
    bool GetNextSDLRect(SGRect *rect);
    bool GetNextSGSpan(SGSpan *span);
};

//---------------------------------------------------------------------
//
// Public function: Gets the next rectangle for the renderer to fill.
// The renderer iteratively calls this function to receive the shape
// stored in the normalized edge list as a series of rectangles. The
// function returns true to indicate that it has successfully
// retrieved a rectangle, or it returns false to indicate that
// no more rectangles are available (because the shape is complete).
// The x-y coordinates in each rectangle are integer values.
//
//---------------------------------------------------------------------

bool Feeder::GetNextSDLRect(SGRect *rect)
{
    // Do any more spans remain in current trapezoid?
    if (_height == 0)
    {
        // No more spans are left in the current trapezoid.
        // Do any more trapezoids remain in the edge list?
        if (_edgeL == 0)
            return false;  // no, the edge list is empty

        // Yes, get next trapezoid from edge list
        _edgeR = _edgeL->next;
        assert(_edgeR != 0);  // edges always come in L/R pairs
        _ytop = _edgeL->ytop;
        _height = _edgeL->dy;
        _xL = _edgeL->xtop;
        _dxL = _edgeL->dxdy;
        _xR = _edgeR->xtop;
        _dxR = _edgeR->dxdy;
        if (_dxL == 0 && _dxR == 0)
        {
            // Special case: an actual rectangle
            rect->x  = _xL >> 16;
            rect->w = (_xR >> 16) - rect->x;
            rect->y = _ytop;
            rect->h = _height;
            _height = 0;
            _edgeL = _edgeR->next;
            return true;
        }
    } 

    // Send next span to renderer
    rect->x  = _xL >> 16;
    rect->w = (_xR >> 16) - rect->x;
    rect->y = _ytop;
    rect->h = 1;
    _xL += _dxL;
    _xR += _dxR;
    ++_ytop;
    if (--_height == 0)
        _edgeL = _edgeR->next;

    return true;
}

//---------------------------------------------------------------------
//
// This version fills in the members of the SGRect structure as though
// it was a Windows GDI RECT structure
//
//---------------------------------------------------------------------

bool Feeder::GetNextGDIRect(SGRect *rect)
{
    // Do any more spans remain in current trapezoid?
    if (_height == 0)
    {
        // No more spans are left in the current trapezoid.
        // Do any more trapezoids remain in the edge list?
        if (_edgeL == 0)
            return false;  // no, the edge list is empty

        // Yes, get next trapezoid from edge list
        _edgeR = _edgeL->next;
        assert(_edgeR != 0);  // edges always come in L/R pairs
        _ytop = _edgeL->ytop;
        _height = _edgeL->dy;
        _xL = _edgeL->xtop;
        _dxL = _edgeL->dxdy;
        _xR = _edgeR->xtop;
        _dxR = _edgeR->dxdy;
        if (_dxL == 0 && _dxR == 0)
        {
            // Special case: an actual rectangle
            rect->x = _xL >> 16;        // RECT.left
            rect->w = _xR >> 16;        // RECT.right
            rect->y = _ytop;            // RECT.top
            rect->h = _ytop + _height;  // RECT.bottom
            _height = 0;
            _edgeL = _edgeR->next;
            return true;
        }
    } 

    // Send next span to renderer
    rect->x = _xL >> 16;  // RECT.left
    rect->w = _xR >> 16;  // RECT.right
    rect->y = _ytop;      // RECT.top
    rect->h = _ytop + 1;  // RECT.bottom
    _xL += _dxL;
    _xR += _dxR;
    ++_ytop;
    if (--_height == 0)
        _edgeL = _edgeR->next;

    return true;
}

//---------------------------------------------------------------------
//
// This version fills in the members of the SGSpan structure, which
// describes a subpixel span in an A-buffer. This function is called
// by a renderer that supports antialiasing.
//
//---------------------------------------------------------------------

bool Feeder::GetNextSGSpan(SGSpan *span)
{
    if (_list == 0 && _edgeL == 0)
        return false;

    // Are any more trapezoids left in the current scan line?
    if (_edgeL == 0)
    {
        // No, the next trapezoid starts on a new scan line
        int yscan = _list->ytop;
        EDGE *p = _list, *q = 0;

        do
        {
            q = p->next;
            p = q->next;
        } while (p != 0 && yscan == p->ytop);
        _edgeL = _list;
        _list = p;
        q->next = 0;
    }

    // Detach topmost span from the next trapezoid in this scan line
    _edgeR = _edgeL->next;
    span->xL = _edgeL->xtop;
    span->xR = _edgeR->xtop;
    span->y = _edgeL->ytop;

    // Are there more spans left in this trapezoid?
    if (_edgeL->dy > 1)
    {
        // Yes, update and save remainder of this trapezoid
        _edgeL->ytop = _edgeR->ytop += 1;
        _edgeL->dy -= 1;
        _edgeR->dy += 1;
        _edgeL->xtop += _edgeL->dxdy;
        _edgeR->xtop += _edgeR->dxdy;

        EDGE *tmp = _edgeL;
        _edgeL = _edgeR->next;
        _edgeR->next = _list;
        _list = tmp;
    }
    else
        _edgeL = _edgeR->next;  // discard empty trapezoid

    return true;
}

//---------------------------------------------------------------------
//
// Uses the qsort function in stdlib.h to sort items in a singly linked
// list of EDGE pairs. The qsort function in stdlib.h is used to sort
// the EDGE pairs in order of ascending ytop values. Parameter plist
// points to the head of the list. Parameter length is the number of
// EDGE pairs in the list. The ysortlist function returns a pointer to
// the head of the new, y-sorted list.
//
//---------------------------------------------------------------------

EDGE* Feeder::ysortlist(EDGE *plist, int length)
{
    if (length < 2)
        return plist;

    int i, count = 0;
    EDGE **ptr = new EDGE*[length];  // pointer array for qsort

    assert(ptr);
    for (EDGE *p = plist; p; p = p->next->next)
        ptr[count++] = p;  // add pointer to next EDGE pair in list

    //assert(count == length);
    qsort(ptr, count, sizeof(EDGE*), ycomp);  // stdlib.h function
    for (i = 1; i < count; i++)
        ptr[i-1]->next->next = ptr[i];  // update links in linked list

    ptr[i-1]->next->next = 0;
    EDGE *tmp = ptr[0];
    delete[] ptr;
    return tmp;
}

//---------------------------------------------------------------------
//
// Polygonal edge manager -- EdgeMgr constructor and destructor
//
//---------------------------------------------------------------------

EdgeMgr::EdgeMgr(Renderer *renderer)
            : _inlist(0), _outlist(0), _cliplist(0), 
              _rendlist(0), _savelist(0), _renderer(0)
{
    _inpool = new POOL;
    _outpool = new POOL;
    _clippool = new POOL;
    _rendpool = new POOL;
    _savepool = new POOL;
    assert(_inpool != 0 && _outpool != 0 && _clippool != 0 && 
           _rendpool != 0 && _savepool != 0);
    if (renderer)
        SetRenderer(renderer);
}

EdgeMgr::~EdgeMgr()
{
    delete _inpool;
    delete _outpool;
    delete _clippool;
    delete _rendpool;
    delete _savepool;
}

//---------------------------------------------------------------------
//
// Protected function: Sets the Renderer object that the ShapeGen
// object will use to draw filled shapes on the display
//
//---------------------------------------------------------------------

void EdgeMgr::SetRenderer(Renderer *renderer)
{
    assert(renderer);
    int yres = renderer->QueryYResolution();
    _yshift = 16 - yres;
    _ybias = FIX_BIAS >> yres;
    _renderer = renderer;
}

//---------------------------------------------------------------------
//
// Protected function: Sets the clipping region to the normalized edge
// list in _outlist and discards the old clipping region. Returns true
// if the new clipping region is not empty; otherwise, returns false.
//
//---------------------------------------------------------------------

bool EdgeMgr::SetClipList()
{
    if (_outlist == 0)
    {
        _cliplist = 0;
        _clippool->Reset();
        return false;  // new clipping region is empty
    }
    _cliplist = _outlist;
    _outlist = 0;
    POOL *swap = _clippool; _clippool = _outpool; _outpool = swap;
    _outpool->Reset();
    return true;
}

//---------------------------------------------------------------------
//
// Protected function: Saves a copy of the current clipping region.
// This saved copy can later be restored by calling the SwapClipRegion
// function. If the current clipping region is not empty, the function
// returns true. Otherwise, it returns false.
//
//---------------------------------------------------------------------

bool EdgeMgr::SaveClipRegion()
{
    EDGE dsthead;
    EDGE *q = &dsthead;

    _savepool->Reset();
    if (_cliplist == 0)
    {
        _savelist = 0;
        return false;
    }
    for (EDGE *p = _cliplist; p != 0; p = p->next)
    {
        q->next = _savepool->Allocate();
        q = q->next;
        *q = *p;
    }
    q->next = 0;
    _savelist = dsthead.next;
    return true;
}

//---------------------------------------------------------------------
//
// Protected function: Swaps the current clipping region with the
// previously saved copy of a clipping region. Only one such copy
// exists at a time. This copy either was swapped out by an earlier
// SwapClipRegion call, or was previously saved by a SaveClipRegion
// call. The SwapClipRegion function returns true if the new
// clipping region is not empty. Otherwise, it returns false.
//
//---------------------------------------------------------------------

bool EdgeMgr::SwapClipRegion()
{
    EDGE *swap = _cliplist;  _cliplist = _savelist;  _savelist = swap;
    POOL *swap2 = _clippool;   _clippool = _savepool;   _savepool = swap2;
    return (_cliplist != 0);
}

//---------------------------------------------------------------------
//
// Protected function: Reverses the direction of each edge in the
// normalized edge list
//
//---------------------------------------------------------------------

void EdgeMgr::ReverseEdges()
{
    for (EDGE *p = _outlist; p; p = p->next)
        p->dy = -p->dy;
}

//---------------------------------------------------------------------
//
// Protected function: Translates the position of each edge in the
// input edge list by the specified x and y displacements
//
//---------------------------------------------------------------------

void EdgeMgr::TranslateEdges(int x, int y)
{
    x = x << 16;
    y = y << (16 - _yshift);

    for (EDGE *p = _inlist; p != 0; p = p->next)
    {
        p->xtop -= x;
        p->ytop -= y;
    }
}

//---------------------------------------------------------------------
//
// Protected function: Clips the newly created normalized edge list in
// _outlist to the current clipping region. On exit, _outlist contains
// the clipped edges.
//
//---------------------------------------------------------------------

void EdgeMgr::ClipEdges(FILLRULE fillrule)
{
    assert(_inlist == 0 && (_inpool->GetCount() == 0));
    assert(fillrule == FILLRULE_INTERSECT || fillrule == FILLRULE_EXCLUDE);

    // The output list may be empty if the path describes a shape
    // so tiny that it falls into a crack between pixels
    if (_outlist == 0)
        return;

    // An empty clip list means the clipping region has no interior,
    // so everything gets clipped and nothing gets drawn
    if (_cliplist == 0)
    {
        _outlist = 0;
        _outpool->Reset();
        return;
    }

    // Swap _inlist and _outlist (and their respective pools)
    _inlist = _outlist;
    _outlist = 0;
    POOL *swap = _inpool; _inpool = _outpool; _outpool = swap;
    
    // Add copy of clipping region from _cliplist to _inlist
    for (EDGE *p = _cliplist; p != 0; p = p->next)
    {
        EDGE *q = _inpool->Allocate();

        *q = *p;
        q->next = _inlist;
        _inlist = q;
    }

    // Get intersection of clip list with normalized edge list
    NormalizeEdges(fillrule);
    _inlist = 0;
    _inpool->Reset();
}

//---------------------------------------------------------------------
//
// Protected function: Fills all the trapezoids defined by the clipped
// and normalized edges in _outlist. Returns true if _outlist is not
// empty, so that one or more trapezoids are filled. If _outlist is
// empty, the function does no fills and immediately returns false.
//
//---------------------------------------------------------------------

bool EdgeMgr::FillEdgeList()
{
    Feeder iter;
    int length;  // this is nonzero only if antialiasing is enabled

    if (_outlist == 0)
        return false;

    _rendlist = _outlist;
    POOL *swap = _outpool;  _outpool = _rendpool;  _rendpool = swap;
    _outlist = 0;
    _outpool->Reset();
    length = (_yshift == 16) ? 0 : _rendpool->GetCount();
    iter.SetEdgeList(_rendlist, length);
    _renderer->RenderShape(&iter);
    return true;
}

//---------------------------------------------------------------------
//
// Protected function: Saves the next pair of mated edges to the
// normalized edge list pointed to by _outlist. Each pair of mated edges
// specifies a trapezoid. Parameter edgeL is a pointer to the edge that
// defines the left side of the trapezoid. Parameter edgeR is a pointer
// to the edge defining the right side.
//
//---------------------------------------------------------------------

void EdgeMgr::SaveEdgePair(int height, EDGE *edgeL, EDGE *edgeR)
{
    EDGE *p = _outpool->Allocate();
    EDGE *q = _outpool->Allocate();

    *p = *edgeL;
    p->dy = +height;  // positive so that winding number increments
    p->next = q;
    *q = *edgeR;
    q->dy = -height;  // negative so that winding number decrements
    q->next = _outlist;
    _outlist = p;
}

//---------------------------------------------------------------------
//
// Protected function: Sets the clipping region to the dimensions of
// the device clipping rectangle. The input width and height values
// are both integers, and must be converted to the internal 16.16
// fixed-point format before the AttachEdge function calls.
//
//---------------------------------------------------------------------

void EdgeMgr::SetDeviceClipRectangle(int width, int height)
{
    VERT16 v1, v2;  // top and bottom ends of vertical edge

    // Discard any previously saved copy of the clipping region
    _savelist = 0;
    _savepool->Reset();

    // Add left and right sides of rectangle to _inpool
    assert(_inlist == 0 && _inpool->GetCount() == 0);
    v1.y = 0;
    v2.y = height << 16;
    v1.x = v2.x = 0;
    AttachEdge(&v1, &v2);  
    v1.x = v2.x = width << 16;
    AttachEdge(&v2, &v1);  // <-- note reverse ordering

    // Swap _inpool with _clippool, and reset _inpool
    _cliplist = _inlist;
    _inlist = 0;
    POOL *swap = _clippool; _clippool = _inpool; _inpool = swap;
    _inpool->Reset();
}

//---------------------------------------------------------------------
//
// Protected function: Partition a complex polygonal shape (consisting
// of perhaps multiple closed figures) into a list of nonoverlapping
// trapezoids that are ready to be filled. The boundaries of each
// trapezoid are determined according to the specified polygon fill
// rule. The polygon may contain holes, disjoint regions, and
// boundary self-intersections. The path manager is responsible for
// ensuring that all figures are closed, which means that the number
// of edges intersected by any scan line is always even, and never odd.
//
//----------------------------------------------------------------------

void EdgeMgr::NormalizeEdges(FILLRULE fillrule)
{
    int y, h, length, yscan, wind;
    FIX16 xdist, ddx;
    EDGE *p, *q, *ylist, *xlist, head;

    assert(_outlist == 0 && _outpool->GetCount() == 0);
    if (_inlist == 0)
        return;  // nothing to do here

    if (_cliplist == 0)
    {
        _inlist = 0;
        _inpool->Reset();
        return;  // nothing to do here
    }

    // Sort input list of edges in order of ascending ytop values
    length = _inpool->GetCount();
    ylist = sortlist(_inlist, length, ycomp);

    // Partition polygon into a list of non-overlapping trapezoids. The
    // trapezoids are filled in major order from top (minimum y) to
    // bottom, and in minor order from left (minimum x) to right on the
    // display. Each iteration below fills a band of trapezoids with
    // the same y = ytop.
    
    xlist = 0;
    while (ylist != 0)
    {
        yscan = ylist->ytop;  // y coordinate at current scan line

        // Starting at the head of the y-sorted list, remove each edge
        // for which ytop == yscan. Set height h to the minimum height
        // of these edges. Form a new, x-sorted list from these edges.
        
        h = BIGVAL16;
        p = ylist;
        length = 0;
        do       
        {
            h = min(h, abs(p->dy));
            q = p->next;    // number of edges is always even
            h = min(h, abs(q->dy));
            length += 2;
        } while ((p = q->next) != 0 && p->ytop == yscan);
        q->next = 0;
        xlist = sortlist(ylist, length, xcomp);
        ylist = p;

        // The x-sorted list represents a band of trapezoids of height h.
        // The number of edges in a band is always even. If the top of
        // the first edge in the y-sorted list intrudes into the band,
        // reduce the band's height h just enough to exclude this edge.
        
        if (ylist != 0)
            h = min(h, ylist->ytop - yscan);

        // If any pair of adjacent edges in the x-sorted list intersect,
        // decrease height h to exclude the point of intersection. Don't
        // bother checking further if h reaches its minimum value of 1.
        
        p = xlist;
        while ((q = p->next) != 0 && h > 1)
        {
            ddx = p->dxdy - q->dxdy;
            xdist = q->xtop - p->xtop;
            if (ddx > 0 && xdist < (h - 1)*ddx)
                h = 1 + xdist/ddx;

            p = q;
        }

        // Use the specified fill rule to identify and render the
        // non-overlapping trapezoids within the current band
        
        p = xlist;
        switch (fillrule)
        {
        case FILLRULE_EVENODD:
            do
            {
                q = p->next;
                SaveEdgePair(h, p, q);
            } while ((p = q->next) != 0);
            break;
        case FILLRULE_WINDING:
            do
            {
                wind = sign(p->dy);
                q = p->next;
                while ((wind += sign(q->dy)) != 0)
                {
                    q = q->next;
                    wind += sign(q->dy);
                    q = q->next;
                }
                SaveEdgePair(h, p, q);
            } while ((p = q->next) != 0);
            break;
        case FILLRULE_INTERSECT:
        case FILLRULE_EXCLUDE:
            wind = (fillrule == FILLRULE_INTERSECT) ? -1 : 0;
            for (;;)
            {
                while (p != 0 && (wind += sign(p->dy)) != 1)
                    p = p->next;
                if (p == 0)
                    break;
                q = p->next;
                while (q != 0 && (wind += sign(q->dy)) != 0)
                    q = q->next; 
                if (q == 0)
                    break;
                SaveEdgePair(h, p, q);
                p = q->next;
            }
            break;
        default:
            assert(0);
            break;
        }

        // For each edge in the x-sorted list, clip off the portion of
        // the edge that lies within the current trapezoid band. Save
        // any of the resulting edges that are of nonzero height.
        
        p = xlist;
        q = &head;
        yscan += h;
        do
        {   
            p->dy -= (p->dy < 0) ? -h : h;
            if (p->dy != 0)
            {
                p->xtop += h*(p->dxdy);
                p->ytop = yscan;
                q->next = p;
                q = p;
            }
        } while ((p = p->next) != 0);

        // All edges remaining in the x-sorted list have the same ytop.
        // Insert this list at the head of the y-sorted list.
        
        if (q != &head)
        {
            q->next = ylist;
            ylist = head.next;
        }
    }
    _inlist = 0;
    _inpool->Reset();
}

//---------------------------------------------------------------------
//
// Protected function: Converts a directed line segment (taken from a
// path) to a polygonal edge and adds the edge to the input edge list.
// Parameters v1 and v2 specify the x-y coordinates of the line start
// and end points, respectively.
//
//----------------------------------------------------------------------

void EdgeMgr::AttachEdge(const VERT16 *v1, const VERT16 *v2)
{
    EDGE *p;
    FIX16 xgap;
    VERT16 vtop, vbot;
    int j, k, y;

    // If edge is horizontal, discard it
    j = (v1->y + _ybias) >> _yshift;
    k = (v2->y + _ybias) >> _yshift;
    if (k == j)
        return;

    // Identify top and bottom vertexes on current edge
    if (k > j)
    {
        vtop = *v1;
        vbot = *v2;
        y = j;
    }
    else
    {
        vtop = *v2;
        vbot = *v1;
        y = k;
    }

    // Create new EDGE structure and insert at head of _inlist
    p = _inpool->Allocate();
    p->dxdy = fixdiv(vbot.x - vtop.x, vbot.y - vtop.y, _yshift);
    xgap = fixmpy(p->dxdy, (y << _yshift) + (_ybias+1) - vtop.y, _yshift);
    p->xtop = vtop.x + xgap + FIX_BIAS;
    p->dy = k - j;
    p->ytop = y;
    p->next = _inlist;
    _inlist = p;
}
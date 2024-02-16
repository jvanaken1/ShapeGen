/*
  Copyright (C) 2022-2024 Jerry R. VanAken

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
// gradient.cpp:
//   Paint generator classes for linear and radial gradient fills
//
//---------------------------------------------------------------------

#include <math.h>
#include <string.h>
#include <assert.h>
#include "renderer.h"

// Color-stop array element (with rgba split into ga and rb)
struct STOP_COLOR
{
    FIX16 offset;  // range 0..1 (fixed-point 0 to 0x0000ffff)
    COLOR ga;      // format = 0x00aa00gg
    COLOR rb;      // format = 0x00bb00rr
};

//---------------------------------------------------------------------
//
// ColorStops class -- The gradient color-stop array manager
//
//---------------------------------------------------------------------

class ColorStops
{
    STOP_COLOR _stop[STOPARRAY_MAXLEN+1];
    int _stopCount, _stopIndex;
    STOP_COLOR _tminStop, _tmaxStop;

    int SetColorStop(int index, FIX16 offset, COLOR color);

public:
    ColorStops()
    {
        ResetColorStops();
    }
    ~ColorStops() {}
    void ResetColorStops();
    bool AddColorStop(float offset, COLOR color);
    COLOR GetPadColor(int n, COLOR opacity);
    COLOR GetColorValue(FIX16 t, COLOR opacity);
};

// Private function: Loads a color-stop array element, identified
// by the 'index' parameter, with the specified 'offset' and 32-bit
// 'color' value. The 'offset' parameter is a 16.16 fixed-point
// value in the range 0 to 1.0 (that is, 0 to 0x0000ffff). The color
// value is nominally in RGBA format (that is, 0xaabbggrr), although
// some renderers might choose to quietly swap the red and blue fields.
// The function premultiplies the RGB fields of the color value by
// its alpha field. All subsequent pixel operations on the color-
// stop array will assume that its color values are in premultiplied-
// alpha format.
int ColorStops::SetColorStop(int index, FIX16 offset, COLOR color)
{
    COLOR rb, ga, a = color >> 24;

    if (a != 0)
    {
        rb = color & 0x00ff00ff;
        ga = (color ^ rb) >> 8;
        if (a != 255)
        {
            rb *= a;
            rb += 0x00800080;
            rb += (rb >> 8) & 0x00ff00ff;
            rb = (rb >> 8) & 0x00ff00ff;
            ga |= 0x00ff0000;
            ga *= a;
            ga += 0x00800080;
            ga += (ga >> 8) & 0x00ff00ff;
            ga = (ga >> 8) & 0x00ff00ff;
        }
    }
    else
        rb = ga = 0;

    _stop[index].ga = ga;
    _stop[index].rb = rb;
    _stop[index].offset = offset;

    // Force color-stop cache miss since color stops have changed
    _tminStop.offset = 0x0000ffff;
    _tmaxStop.offset = 0;
    return ++index;
}

// Public function: Deletes all existing color stops from color-stop
// array, and loads a valid -- but temporary -- set of color stops to
// use until the user calls the AddColorStop function to define a new
// set of color stops.
void ColorStops::ResetColorStops()
{
    const STOP_COLOR STOP000 = { 0,0,0 };
    _stop[0] = STOP000;
    _stopIndex = 0;
    _stopCount = SetColorStop(1, 0x0000ffff, 0);
}

// Public function: Adds a color stop to the color-stop array. The
// problem is that this function can be called at any time to add a new
// color stop. But the color-stop array must always be in a state in
// which it's ready to handle a color look-up request; in particular,
// the first and last array elements must have respective offsets of 0
// and 1.0 (represented as fixed-point values 0 and 0x0000ffff). The
// solution is to have two distinct array indices: _stopIndex is used
// to track the caller's additions to the array, but _stopCount can
// point to an additional array element, if necessary, so that the
// offset in the final element is 1.0, as required for lookups.
bool ColorStops::AddColorStop(float offset, COLOR color)
{
    // Prevent color-stop array from overflowing
    if (_stopIndex == ARRAY_LEN(_stop))
        return false;

    // Update _stopIndex to make room for user's new color stop
    FIX16 fixoff = 0x0000ffff*offset;  // convert to fixed-point

    if (fixoff < 0)
        fixoff = 0;
    else if (fixoff > 0x0000ffff)
        fixoff = 0x0000ffff;

    if (_stopIndex == 0)  // initial color-stop entry?
    {
        _stopIndex = SetColorStop(0, 0, color);
        if (fixoff > 0)
            _stopIndex = SetColorStop(1, fixoff, color);
    }
    else
    {
        if (_stopIndex == ARRAY_LEN(_stop)-1)
            fixoff = 0x0000ffff;
        else if (fixoff < _stop[_stopIndex-1].offset)
            fixoff = _stop[_stopIndex-1].offset;

        _stopIndex = SetColorStop(_stopIndex, fixoff, color);
    }

    // Make sure final color-stop element has offset of 1.0
    // (represented in fixed-point as 0x0000ffff)
    if (fixoff == 0x0000ffff)
        _stopCount = _stopIndex;
    else
        _stopCount = SetColorStop(_stopIndex, 0x0000ffff, color);

    return true;
}

// Public function: Returns the indicated pad color. The user calls
// this function to enable the SPREAD_PAD spread method. Parameter 'n'
// indicates whether to retrieve the padding color for the inside of
// the starting circle (n < 0) or the outside of the ending circle
// (n > 0). The 'opacity' parameter is an alpha value in the range 0 to
// 255. The returned color value is always in premultiplied-alpha
// format, so the function multiplies all four components (r,g,b,a) of
// this value by 'opacity' before returning.
COLOR ColorStops::GetPadColor(int n, COLOR opacity)
{
    STOP_COLOR& stop = (n < 0) ? _stop[0] : _stop[_stopCount-1];
    COLOR rb = stop.rb, ga = stop.ga;

    if (opacity == 255)
        return (ga << 8) | rb;

    rb *= opacity;
    rb += 0x00800080;
    rb += (rb >> 8) & 0x00ff00ff;
    rb = (rb >> 8) & 0x00ff00ff;
    ga *= opacity;
    ga += 0x00800080;
    ga += (ga >> 8) & 0x00ff00ff;
    ga &= 0xff00ff00;
    return ga | rb;
}

// Public function: Calculates the color of a pixel given (1) the color
// look-up parameter 't' at the pixel's center, and (2) the input
// opacity, which is an alpha value in the range 0 to 255. Parameter
// 't' is a 16.16 fixed-point fraction in the range 0 to 1.0 (that is,
// 0 to 0x0000ffff) that indicates where the pixel center falls within
// the repeating color-stop pattern. All four of the pixel's 8-bit
// (r,g,b,a) components by are then multiplied by 'opacity'. The two
// color stops that bracket the specified 't' value are cached so that
// subsequent pixel color lookups might re-use the cached color stops.
COLOR ColorStops::GetColorValue(FIX16 t, COLOR opacity)
{
    assert((t >> 16) == 0 && opacity != 0 && (opacity >> 8) == 0);
    if (t < _tminStop.offset || _tmaxStop.offset < t)
    {
        // A color-stop cache miss has occurred. Update the two
        // cached color stops to bracket the new t value.
        int k;
        for (k = 1; k < _stopCount; ++k)
        {
            if (t <= _stop[k].offset && _stop[k-1].offset < _stop[k].offset)
                break;
        }
        _tminStop = _stop[k-1];
        _tmaxStop = _stop[k];
    }

    // Linearly interpolate between the two cached color stops on
    // either side of t. The RGB components in the two color stops
    // have previously been premultiplied by their alphas.
    COLOR ga1 = _tminStop.ga;
    COLOR ga2 = _tmaxStop.ga;
    if ((ga1 | ga2) == 0)
        return 0;  // pixel is transparent

    float width = _tmaxStop.offset - _tminStop.offset;
    FIX16 s = 0x0000ffff*((t - _tminStop.offset)/width);
    s >>= 8;
    COLOR rb1 = _tminStop.rb;
    COLOR rb2 = _tmaxStop.rb;
    COLOR rb = 256*rb1 - rb1 + s*(rb2 - rb1);
    COLOR ga = 256*ga1 - ga1 + s*(ga2 - ga1);
    rb += 0x00800080;
    rb += (rb >> 8) & 0x00ff00ff;
    rb = (rb >> 8) & 0x00ff00ff;
    ga += 0x00800080;
    ga += (ga >> 8) & 0x00ff00ff;
    ga &= 0xff00ff00;
    if (opacity == 255)
        return ga | rb;

    // Multiply all four components (r,g,b,a) by 'opacity'
    rb *= opacity;
    rb += 0x00800080;
    rb += (rb >> 8) & 0x00ff00ff;
    rb = (rb >> 8) & 0x00ff00ff;
    ga = opacity*(ga >> 8);
    ga += 0x00800080;
    ga += (ga >> 8) & 0x00ff00ff;
    ga &= 0xff00ff00;
    return ga | rb;
}

//---------------------------------------------------------------------
//
// LinearGrad class -- Paint generator for linear gradient fills
//
//---------------------------------------------------------------------

class LinearGrad : public LinearGradient
{
    ColorStops *_cstops;    // color-stop manager object
    float _x0, _y0;         // starting point (t = 0)
    float _x1, _y1;         // ending point (t = 1)
    SPREAD_METHOD _spread;  // pad, repeat, or reflect
    bool _bExtStart;        // extend past starting point (t < 0)
    bool _bExtEnd;          // extend past ending point (t >= 1)
    COLOR _startColor;      // starting color if SPREAD_PAD
    COLOR _endColor;        // ending color if SPREAD_PAD
    int _xscroll, _yscroll; // scroll position coordinates
    bool _bSpecial;         // special case x0==x1 and y0==y1

    // These values are constant over the lifetime of the object
    float _dtdx;  // partial derivative dt/dx
    float _dtdy;  // partial derivative dt/dy

public:
    LinearGrad() : _cstops(0)
    {
        assert(_cstops != 0);
    }
    LinearGrad(float x0, float y0, float x1, float y1,
               SPREAD_METHOD spread, int flags, const float xform[6]);
    ~LinearGrad()
    {
        delete _cstops;
    }
    bool GetStatus()
    {
        return (_cstops != 0);  // did constructor succeed?
    }
    void FillSpan(int xs, int ys, int length, COLOR outBuf[], const COLOR inAlpha[]);
    bool AddColorStop(float offset, COLOR color)
    {
        return _cstops->AddColorStop(offset, color);
    }
    bool SetScrollPosition(int x, int y)
    {
        _xscroll = x, _yscroll = y;
        return true;
    }
};

// Constructor: Defines a new linear-gradient fill pattern
LinearGrad::LinearGrad(float x0, float y0, float x1, float y1,
                       SPREAD_METHOD spread, int flags, const float xform[6]) :
              _x0(x0), _y0(y0), _x1(x1), _y1(y1), _xscroll(0), _yscroll(0)
{
    _cstops = new ColorStops();
    assert(_cstops);  // out of memory?
    _bExtStart = (flags & FLAG_EXTEND_START) != 0;
    _bExtEnd = (flags & FLAG_EXTEND_END) != 0;
    switch (spread)
    {
    case SPREAD_PAD:
    case SPREAD_REFLECT:
        _spread = spread;
        break;
    default:
        _spread = SPREAD_REPEAT;
        break;
    }
    _bSpecial = (_x0 == _x1) && (_y0 == _y1);
    if (_bSpecial)
        return;

    if (xform != 0)
    {
        // Apply the user-specified affine transformation matrix to
        // start point (x0,y0) and end point (x1,y1)
        float tmp;

        tmp = xform[0]*_x0 + xform[2]*_y0 + xform[4];
        _y0 = xform[1]*_x0 + xform[3]*_y0 + xform[5];
        _x0 = tmp;
        tmp = xform[0]*_x1 + xform[2]*_y1 + xform[4];
        _y1 = xform[1]*_x1 + xform[3]*_y1 + xform[5];
        _x1 = tmp;
    }

    // Calculation of the gradient pattern will be simplified by
    // placing the origin at (x0,y0)
    _x1 -= _x0, _y1 -= _y0;
    _x0 -= 0.5, _y0 -= 0.5;

    // Calculate partial derivatives dt/dx and dt/dy
    float dist2 = _x1*_x1 + _y1*_y1;

    _dtdx = _x1/dist2;
    _dtdy = _y1/dist2;
}

// Public function: Fills the pixels in a single horizontal span with
// a linear gradient pattern. The span starts at pixel (xs,ys) and
// extends 'len' pixels to the right. The function writes the processed
// gradient-fill pixels to the outBuf array. To support shape anti-
// aliasing and source constant alpha, the inAlpha array contains
// 8-bit alpha values to apply to the gradient-fill pixels (in addition
// to the per-pixel alphas in the gradient).
void LinearGrad::FillSpan(int xs, int ys, int len, COLOR outBuf[], const COLOR inAlpha[])
{
    // Special case: x0 == x1 and y0 == y1
    if (_bSpecial)
    {
        if (_bExtEnd)
        {
            for (int i = 0; i < len; ++i)
            {
                COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];
                COLOR color = _cstops->GetPadColor(1, opacity);

                outBuf[i] = color;
            }
        }
        return;
    }

    float xp = xs - _x0 + _xscroll;
    float yp = ys - _y0 + _yscroll;
    float t = xp*_dtdx + yp*_dtdy;

    // Normal case: Each iteration of this for-loop paints one pixel
    for (int i = 0; i < len; ++i)
    {
        COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];

        if (opacity != 0)
        {
            COLOR color = 0;
            bool bValid = ((_bExtStart || t >= 0) && (_bExtEnd || t < 1.0));

            if (bValid)
            {
                int n = t;

                if (t < 0) --n;
                if (_spread == SPREAD_PAD && n != 0)
                    color = _cstops->GetPadColor(n, opacity);
                else
                {
                    // Convert t from float to 16.16 fixed-point format. We
                    // represent 1.0 as 0x0000ffff instead of as 0x00010000
                    // to help distinguish 1.0 from 0 at boundaries between
                    // color patterns when spread == SPREAD_REFLECT.
                    FIX16 tfix = 0x0000ffff*(t - n);

                    if (_spread == SPREAD_REFLECT && (n & 1))
                        tfix ^= 0x0000ffff;

                    color = _cstops->GetColorValue(tfix, opacity);
                }
            }
            outBuf[i] = color;
        }
        t += _dtdx;
    }
}

// Called by a renderer to create a new linear-gradient object
LinearGradient* CreateLinearGradient(float x0, float y0, float x1, float y1,
                                     SPREAD_METHOD spread, int flags,
                                     const float xform[6])
{
    LinearGrad *grad = new LinearGrad(x0, y0, x1, y1, spread, flags, xform);
    if (grad == 0 || grad->GetStatus() == false)
    {
        assert(grad != 0 && grad->GetStatus() == true);
        delete grad;
        return 0;  // constructor failed
    }
    return grad;  // success
}

//---------------------------------------------------------------------
//
// RadialGrad class -- Paint generator for radial gradient fills
//
//---------------------------------------------------------------------

class RadialGrad : public RadialGradient
{
    ColorStops *_cstops;    // color-stop manager object
    float _x0, _y0, _r0;    // starting circle
    float _x1, _y1, _r1;    // ending circle
    SPREAD_METHOD _spread;  // pad, repeat, or reflect
    bool _bExtStart;        // extend starting circle to t < 0
    bool _bExtEnd;          // extend ending circle to t >= 1
    bool _bSpecial;         // special case (x0,y0,r0) = (x1,y1,r1)
    float _vx, _vy;         // y-scaling and x-shear
    COLOR _startColor;      // starting color if SPREAD_PAD
    COLOR _endColor;        // ending color if SPREAD_PAD
    int _xscroll, _yscroll; // scroll position coordinates

    // These values are constant over the lifetime of the object
    float _dr;     // dr = r1 - r0
    float _a;      // a = x1^2 + y1^2 - dr^2
    float _inva;   // inva = 1/a
    float _A2;     // A2 = dr^2 - y1^2

    void TransformRadialGradient(const float xform[6]);

public:
    RadialGrad() : _cstops(0)
    {
        assert(_cstops != 0);
    }
    RadialGrad(float x0, float y0, float r0, float x1, float y1, float r1,
               SPREAD_METHOD spread, int flags, const float xform[6]);
    ~RadialGrad()
    {
        delete _cstops;
    }
    bool GetStatus()
    {
        return (_cstops != 0);  // did constructor succeed?
    }
    void FillSpan(int xs, int ys, int length, COLOR outBuf[], const COLOR inAlpha[]);
    bool AddColorStop(float offset, COLOR color)
    {
        return _cstops->AddColorStop(offset, color);
    }
    bool SetScrollPosition(int x, int y)
    {
        _xscroll = x, _yscroll = y;
        return true;
    }
};

// Constructor: Defines a new radial-gradient fill pattern
RadialGrad::RadialGrad(float x0, float y0, float r0, float x1, float y1, float r1,
                       SPREAD_METHOD spread, int flags, const float xform[6]) :
                 _x0(x0), _y0(y0), _r0(r0), _x1(x1), _y1(y1), _r1(r1),
                 _xscroll(0), _yscroll(0), _vx(0), _vy(0)
{
    assert((r0 >= 0) && (r1 >= 0) && ((r0 != 0) || (r1 != 0)));
    _cstops = new ColorStops();
    assert(_cstops);  // out of memory?
    _bExtStart = (flags & FLAG_EXTEND_START) != 0;
    _bExtEnd = (flags & FLAG_EXTEND_END) != 0;
    switch (spread)
    {
    case SPREAD_PAD:
    case SPREAD_REFLECT:
        _spread = spread;
        break;
    default:
        _spread = SPREAD_REPEAT;
        break;
    }
    _bSpecial = (x0 == x1) && (y0 == y1) && (r0 == r1);
    if (_bSpecial)
        return;

    if (xform != 0)
    {
        TransformRadialGradient(xform);
        if (_bSpecial)
            return;
    }
    else
        _vx = 0, _vy = 1.0;

    // Calculation of the gradient pattern will be simplified by
    // placing the origin at (x0,y0). Also, if the caller supplied
    // an affine transformation matrix, any required scaling or
    // shear has been incorporated into the vector (vx,vy).
    _x1 -= _x0, _y1 -= _y0;
    _x1 += _vx*_y1, _y1 *= _vy;
    _x0 -= 0.5, _y0 -= 0.5;

    // The values below are constant over this object's lifetime
    _dr = _r1 - _r0;
    _a = _x1*_x1 + _y1*_y1 - _dr*_dr;
    _inva = (_a == 0) ? 0 : 1.0/_a;
    _A2 = _dr*_dr - _y1*_y1;
}

// Private function: Transforms the radial gradient fill pattern.
// If the user provides an affine transformation matrix, this
// function is called by the constructor to apply the specified
// transformation to the radial gradient parameters.
void RadialGrad::TransformRadialGradient(const float xform[6])
{
    float px, py, qx, qy, tmp, rmax, rmin, ratio;

    if (_r1 > _r0)
        rmax = _r1, rmin = _r0;
    else
        rmax = _r0, rmin = _r1;

    ratio = rmin/rmax;

    // Apply the user-specified affine transformation matrix to
    // circle centers (x0,y0) and (x1,y1). Also, convert circle
    // radius rmax into conjugate diameter end points (px,py)
    // and (qx,qy) of the corresponding transformed ellipse.
    tmp = xform[0]*_x0 + xform[2]*_y0 + xform[4];
    _y0 = xform[1]*_x0 + xform[3]*_y0 + xform[5];
    _x0 = tmp;
    tmp = xform[0]*_x1 + xform[2]*_y1 + xform[4];
    _y1 = xform[1]*_x1 + xform[3]*_y1 + xform[5];
    _x1 = tmp;
    px = xform[0]*rmax, py = xform[1]*rmax;
    qx = xform[2]*rmax, qy = xform[3]*rmax;

    // Initialize unit vectors u and v to point in the x and y
    // directions, respectively. These reference vectors will
    // track the cumulative effects of the transformations below.
    float ux = 1.0, uy = 0;
    _vx = 0, _vy = 1.0;

    // Derive coefficients A, B, and C of the implicit equation
    // for the ellipse: f(x,y) = Ax^2 + Bxy + Cy^2 + ... = 0
    float B = -2.0*(px*py + qx*qy);

    if (B != 0)
    {
        float A = py*py + qy*qy;
        float C = px*px + qx*qx;
        float beta = (C - A)/B;
        float slope = beta + sqrt(beta*beta + 1.0);
        float norm = sqrt(slope*slope + 1.0);

        // Rotate the ellipse so that its principal axes are
        // aligned with the x-y axes. Apply the same rotation
        // to unit vectors u and v.
        float cosa = 1.0/norm, sina = slope/norm;

        tmp = px*cosa + py*sina;
        py = -px*sina + py*cosa;
        px = tmp;
        tmp = qx*cosa + qy*sina;
        qy = -qx*sina + qy*cosa;
        qx = tmp;
        tmp = ux*cosa + uy*sina;
        uy = -ux*sina + uy*cosa;
        ux = tmp;
        tmp = _vx*cosa + _vy*sina;
        _vy = -_vx*sina + _vy*cosa;
        _vx = tmp;
    }

    // Scale px and qx to squeeze or stretch the axis-aligned ellipse
    // in the x-dimension to transform it into a circle. Apply the
    // same scaling to vectors u and v; as a result, u and v will no
    // longer be unit vectors. However, vectors p and q will once
    // again be mutually perpendicular (although they will not, in
    // general, be unit vectors or aligned to the x-y axes).
    float dx, dy, scale;

    dx = fabs(px) + fabs(qx);
    if (dx < 0.00001f)
    {
        _bSpecial = true;  // degenerate ellipse
        return;
    }
    dy = fabs(py) + fabs(qy);
    scale = dy/dx;
    px *= scale, qx *= scale;
    rmax = sqrt(px*px + py*py);
    ux *= scale, _vx *= scale;

    // Scale vector u back to unit-vector size, and scale
    // vector v and radii r1 and r0 by the same amount
    scale = 1.0/sqrt(ux*ux + uy*uy);
    ux *= scale, uy *= scale;
    _vx *= scale, _vy *= scale;
    rmax *= scale;
    rmin = ratio*rmax;
    if (_r1 > _r0)
        _r1 = rmax, _r0 = rmin;
    else
        _r0 = rmax, _r1 = rmin;

    // Rotate unit vector u so that it's horizontal and points in the
    // +x direction. Rotate vector v by the same angle. Since we know
    // the new u is always (1,0), we need to save only the new v.
    float cosb = ux, sinb = uy;

    tmp = _vx*cosb + _vy*sinb;
    _vy = -_vx*sinb + _vy*cosb;
    _vx = tmp;
}

// Public function: Fills the pixels in a single horizontal span with
// a radial gradient pattern. The span starts at pixel (xs,ys) and
// extends 'len' pixels to the right. The function writes the processed
// gradient-fill pixels to the 'outBuf' array. To support shape anti-
// aliasing and source constant alpha, the 'inAlpha' array contains
// 8-bit alpha values to apply to the gradient-fill pixels (in addition
// to the per-pixel alphas in the gradient). The constructor previously
// set the values of constants _dr, _a, _inva, and _A2.
void RadialGrad::FillSpan(int xs, int ys, int len, COLOR outBuf[], const COLOR inAlpha[])
{
    float xp, yp, b0, b, phi, A0, A1;

    // Special case: x0 == x1, y0 == y1, and r0 == r1
    if (_bSpecial)
    {
        if (_bExtEnd)
        {
            for (int i = 0; i < len; ++i)
            {
                COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];
                COLOR color = _cstops->GetPadColor(1, opacity);

                outBuf[i] = color;
            }
        }
        return;
    }

    xp = xs - _x0 + _xscroll;
    yp = ys - _y0 + _yscroll;
    xp += _vx*yp, yp *= _vy;  // apply scaling + shearing transform
    b0 = yp*_y1 + _r0*_dr;
    b = xp*_x1 + b0;
    phi = yp*yp - _r0*_r0;
    A0 = b0*b0 - _a*phi;
    A1 = 2*b0*_x1;

    // Normal case: Each iteration of this for-loop paints one pixel
    for (int i = 0; i < len; ++i)
    {
        COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];

        if (opacity != 0)
        {
            COLOR color = 0;
            float discr = (_A2*xp + A1)*xp + A0;

            if (discr < 0 && _a < 0)
                discr = 0;

            if (discr >= 0)
            {
                float t0 = 0, t1 = 0;
                bool bValid0 = false, bValid1 = false;

                if (_a == 0)
                {
                    if (b != 0)
                    {
                        float c = xp*xp + phi;

                        t1 = (c/b)/2;
                        bValid1 = ((_bExtStart || t1 >= 0) &&
                                   (_bExtEnd || t1 < 1.0f) &&
                                   _r0 + t1*_dr > 0);
                    }
                }
                else
                {
                    float root = sqrt(discr);

                    if (_a > 0 || _dr < 0)
                    {
                        t0 = _inva*(b + root);
                        bValid0 = ((_bExtStart || t0 >= 0) &&
                                   (_bExtEnd || t0 < 1.0f) &&
                                   (_a < 0 || _r0 + t0*_dr >= 0));
                    }
                    if (_a > 0 || _dr > 0)
                    {
                        t1 = _inva*(b - root);
                        bValid1 = ((_bExtStart || t1 >= 0) &&
                                   (_bExtEnd || t1 < 1.0f) &&
                                   (_a < 0 || _r0 + t1*_dr >= 0));
                    }
                }
                if (bValid0 || bValid1)
                {
                    float t;
                    int n;

                    if (bValid0 && bValid1)
                        t = (t0 > t1) ? t0 : t1;
                    else
                        t = bValid0 ? t0 : t1;

                    n = t;
                    if (t < 0) --n;
                    if (_spread == SPREAD_PAD && n != 0)
                        color = _cstops->GetPadColor(n, opacity);
                    else
                    {
                        // Convert t from float to 16.16 fixed-point format. We
                        // represent 1.0 as 0x0000ffff instead of as 0x00010000
                        // to help distinguish 1.0 from 0 at boundaries between
                        // color patterns when spread == SPREAD_REFLECT.
                        FIX16 tfix = 0x0000ffff*(t - n);

                        if (_spread == SPREAD_REFLECT && (n & 1))
                            tfix ^= 0x0000ffff;

                        color = _cstops->GetColorValue(tfix, opacity);
                    }
                }
            }
            outBuf[i] = color;
        }
        xp += 1.0f;
        b += _x1;
    }
}

// Called by a renderer to create a new radial-gradient object
RadialGradient* CreateRadialGradient(float x0, float y0, float r0,
                                     float x1, float y1, float r1,
                                     SPREAD_METHOD spread, int flags,
                                     const float xform[6])
{
    RadialGrad *grad = new RadialGrad(x0, y0, r0, x1, y1, r1, spread, flags, xform);
    if (grad == 0 || grad->GetStatus() == false)
    {
        assert(grad != 0 && grad->GetStatus() == true);
        delete grad;
        return 0;  // constructor failed
    }
    return grad;  // success
}

//---------------------------------------------------------------------
//
// ConicGrad class -- Paint generator for conic gradient fills
//
//---------------------------------------------------------------------

namespace {
    // This function is used in the ConicGrad::FillPath() function's
    // inner loop in place of the atan2() function (in the C Standard
    // Library header math.h). Though it's not as accurate as atan2(),
    // it's faster. The source paper for the arctangent formula below
    // is "Efficient Approximations for the Arctangent Function" by S.
    // Rajan, et al. IEEE Signal Processing Magazine, May 2006, p.
    // 108-111. This formula has a maximum absolute error of 0.0015
    // radians (0.086 deg) and uses one divide and three multiplies.
    //
    float my_atan2(float y, float x)
    {
        if (y == 0)
            return (x < 0) ? PI : 0;

        float xabs = (x < 0) ? -x : x;
        float yabs = (y < 0) ? -y : y;
        float z = (xabs < yabs) ? xabs/yabs : yabs/xabs;
        float r = z*(PI/4 + (1 - z)*(0.2447f + 0.0663f*z));

        if (xabs < yabs)
            r = PI/2 - r;
        if (x < 0)
            r = PI - r;
        return (y < 0) ? -r : r;
    }
} // end namespace

class ConicGrad : public ConicGradient
{
    ColorStops *_cstops;    // color-stop manager object
    float _x0, _y0;         // center coordinates
    float _astart;          // starting angle in radians
    float _asweep;          // sweep angle in radians
    float _tstart;          // normalized starting angle
    float _tsweep;          // normalized sweep angle
    float _tmult;           // 1.0/tsweep
    SPREAD_METHOD _spread;  // pad, repeat, or reflect
    int _extend;            // select pad color stop
    float _vx, _vy;         // y-scaling and x-shear
    COLOR _startColor;      // starting color if SPREAD_PAD
    COLOR _endColor;        // ending color if SPREAD_PAD
    int _xscroll, _yscroll; // scroll position coordinates
    bool _bSpecial;         // special case

    void TransformConicGradient(const float xform[6]);

public:
    ConicGrad() : _cstops(0)
    {
        assert(_cstops != 0);
    }
    ConicGrad(float x0, float y0, float astart, float asweep,
              SPREAD_METHOD spread, int flags, const float xform[6]);
    ~ConicGrad()
    {
        delete _cstops;
    }
    bool GetStatus()
    {
        return (_cstops != 0);  // did constructor succeed?
    }
    void FillSpan(int xs, int ys, int length, COLOR outBuf[], const COLOR inAlpha[]);
    bool AddColorStop(float offset, COLOR color)
    {
        return _cstops->AddColorStop(offset, color);
    }
    bool SetScrollPosition(int x, int y)
    {
        _xscroll = x, _yscroll = y;
        return true;
    }
};

// Constructor: Defines a new conic-gradient fill pattern
ConicGrad::ConicGrad(float x0, float y0, float astart, float asweep,
                     SPREAD_METHOD spread, int flags, const float xform[6]) :
             _x0(x0), _y0(y0), _astart(astart), _asweep(asweep),
             _xscroll(0), _yscroll(0), _vx(0), _vy(0), _extend(0),
             _spread(SPREAD_REPEAT), _bSpecial(false)
{
    _cstops = new ColorStops();
    assert(_cstops);  // out of memory?
    _bSpecial = (fabs(_asweep) < 0.00001f);
    if (_bSpecial)
        return;

    if ((flags & FLAG_EXTEND_END) != 0)
        _extend = 1;  // extend color pattern to t > 1
    else if ((flags & FLAG_EXTEND_START) != 0)
    {
        _extend = -1;  // extend color pattern to t < 0
        _astart += _asweep;
        _asweep = -_asweep;
    }
    switch (spread)
    {
    case SPREAD_PAD:
    case SPREAD_REFLECT:
        _spread = spread;
        break;
    default:
        _spread = SPREAD_REPEAT;
        break;
    }
    if (xform != 0)
    {
        TransformConicGradient(xform);
        if (_bSpecial)
            return;
    }
    else
        _vx = 0, _vy = 1.0;

    // Convert caller-specified start and sweep angles from radians to
    // to normalized values, where an angle phi in the range 0 to 2*PI
    // radians corresponds to a normalized angle t in the unit interval
    // [0,1]. If a normalized start angle 'tstart' lies outside this
    // interval, only the fractional part of 'tstart' is preserved. If
    // a normalized sweep angle 'tsweep' lies above the interval [0,1],
    // it is clamped to the value 1. If 'tsweep' lies below the
    // interval [-1,0], it is clamped to the value -1.
    _tstart = _astart/(2*PI);
    _tsweep = _asweep/(2*PI);

    int n = _tstart;

    if (_tstart < 0)
        --n;
    _tstart -= n;

    if (_tsweep > 1.0f)
        _tsweep = 1.0f;
    else if (_tsweep < -1.0f)
        _tsweep = -1.0f;

    _tmult = 1.0f/_tsweep;
}

// Private function: Determines how to transform the conic gradient
// fill pattern. If the user provides an affine transformation matrix,
// this function is called by the constructor to apply the specified
// transformation to the conic gradient parameters.
void ConicGrad::TransformConicGradient(const float xform[6])
{
    float px, py, qx, qy, tmp;

    // Apply the user-specified affine transformation matrix to
    // center coordinates (x0,y0). Apply the same transformation
    // matrix (excluding the x- and y-translation elements) to
    // mutually perpendicular unit vectors p = (px,py) = (1,0)
    // and q = (qx,qy) = (0,1), which are also endpoints of
    // conjugate diameters of the unit circle. After p and q are
    // transformed below, they may no longer be unit vectors or
    // be mutually perpendicular, but they will still be conjugate
    // diameter endpoints of the transformed circle, which is an
    // ellipse.
    tmp = xform[0]*_x0 + xform[2]*_y0 + xform[4];
    _y0 = xform[1]*_x0 + xform[3]*_y0 + xform[5];
    _x0 = tmp;
    px = xform[0], py = xform[1];
    qx = xform[2], qy = xform[3];

    // Initialize unit vectors u and v to point in the x and y
    // directions, respectively. These reference vectors will
    // accumulate any scaling and shearing resulting from the
    // transformations that follow below.
    float ux = 1.0, uy = 0;
    _vx = 0, _vy = 1.0;

    // Derive coefficients A, B, and C of the implicit equation
    // for the ellipse: f(x,y) = Ax^2 + Bxy + Cy^2 + ... = 0. If
    // B == 0, the ellipse's principle axes are already aligned
    // with the x-y coordinate axes.
    float B = -2.0*(px*py + qx*qy);

    if (B != 0)
    {
        float A = py*py + qy*qy;
        float C = px*px + qx*qx;
        float beta = (C - A)/B;
        float slope = beta + sqrt(beta*beta + 1.0);
        float norm = sqrt(slope*slope + 1.0);

        // Rotate the ellipse so that its principal axes are
        // aligned with the x and y coordinate axes. Apply the
        // same rotation to unit vectors u and v.
        float cosa = 1.0/norm, sina = slope/norm;

        tmp = px*cosa + py*sina;
        py = -px*sina + py*cosa;
        px = tmp;
        tmp = qx*cosa + qy*sina;
        qy = -qx*sina + qy*cosa;
        qx = tmp;
        tmp = ux*cosa + uy*sina;
        uy = -ux*sina + uy*cosa;
        ux = tmp;
        tmp = _vx*cosa + _vy*sina;
        _vy = -_vx*sina + _vy*cosa;
        _vx = tmp;
    }

    // The principal axes of the ellipse are now aligned with the x-y
    // coordinate axes. Scale p and q to squeeze or stretch the ellipse
    // in the x-dimension to transform it into a circle. Apply the same
    // scaling to vectors u and v. As a result, u and v may no longer
    // be unit vectors or mutually perpendicular. However, vectors p
    // and q will once again be mutually perpendicular (although they
    // will not, in general, be unit vectors or xy-axis-aligned).
    float dx, dy, scale;

    dx = fabs(px) + fabs(qx);
    if (dx < 0.00001f)
    {
        _bSpecial = true;  // degenerate case
        return;
    }
    dy = fabs(py) + fabs(qy);
    scale = dy/dx;
    px *= scale, qx *= scale;
    ux *= scale, _vx *= scale;

    // Scale vector u back to unit-vector size, and scale
    // vectors v, p, and q by the same amount
    scale = 1.0/sqrt(ux*ux + uy*uy);
    ux *= scale, uy *= scale;
    _vx *= scale, _vy *= scale;
    px *= scale, py *= scale;
    //qx *= scale, qy *= scale;

    // Rotate unit vector u so that it is horizontal and points in
    // the +x direction. Rotate vector v by the same angle. (Since
    // we know that the new u is always (1,0), we need to save
    // only the new v.) Also rotate vectors p and q by this angle.
    float cosb = ux, sinb = uy;

    //tmp = ux*cosb + uy*sinb;
    //uy = -ux*sinb + uy*cosb;
    //ux = tmp;

    tmp = _vx*cosb + _vy*sinb;
    _vy = -_vx*sinb + _vy*cosb;
    _vx = tmp;

    tmp = px*cosb + py*sinb;
    py = -px*sinb + py*cosb;
    px = tmp;

    //tmp = qx*cosb + qy*sinb;
    //qy = -qx*sinb + qy*cosb;
    //qx = tmp;

    // Measure the angle phi through which perpendicular vectors p and
    // q have rotated away from the x and y axes, respectively. This
    // value will be used to compensate for the change in starting
    // angle resulting from transformation matrix xform[].
    _astart += atan2(py, px);
}

// Public function: Fills the pixels in a single horizontal span with
// a conic gradient pattern. The span starts at pixel (xs,ys) and
// extends 'len' pixels to the right. The function writes the processed
// gradient-fill pixels to the 'outBuf' array. To support shape anti-
// aliasing and source constant alpha, the 'inAlpha' array contains
// 8-bit alpha values to apply to the gradient-fill pixels (in addition
// to the per-pixel alphas in the gradient).
void ConicGrad::FillSpan(int xs, int ys, int len, COLOR outBuf[], const COLOR inAlpha[])
{
    float xp, yp;

    // Special case: asweep == 0 or transformed pattern is degenerate
    if (_bSpecial)
    {
        if (_extend)
        {
            for (int i = 0; i < len; ++i)
            {
                COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];
                COLOR color = _cstops->GetPadColor(1, opacity);

                outBuf[i] = color;
            }
        }
        return;
    }

    xp = xs - _x0 + _xscroll;
    yp = ys - _y0 + _yscroll;
    xp += _vx*yp, yp *= _vy;  // apply scaling + shearing transform

    // Normal case: Each iteration of this for-loop paints one pixel
    for (int i = 0; i < len; ++i)
    {
        COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];

        if (opacity != 0)
        {
            // Get the angle 'phi' of this pixel relative to center
            // coordinates (x0,y0). Normalize the angle so that an
            // angle 'phi' in the range 0 to 2*PI radians is mapped
            // to unit interval [0,1]. Note that if the sweep angle
            // is negative, the rotational direction of increasing t
            // is opposite the direction for a positive sweep angle.

            float phi = my_atan2(yp, xp);
            float t = phi/(2*PI);

            if (t < 0)
                t += 1.0f;

            t -= _tstart;
            if (t < 0)
                t += 1.0f;
            else if (t >= 1.0f)
                t -= 1.0f;

            if (t < 0 || t >= 1.0f)  // handle tiny precision errors
                    t = 0;

            if (_tsweep < 0 && t > 0)
                t -= 1.0f;

            // Convert t from float to 16.16 fixed-point format. We
            // represent 1.0 as 0x0000ffff instead of as 0x00010000
            // to help distinguish 1.0 from 0 at boundaries between
            // color pattern cycles when spread == SPREAD_REFLECT.
            FIX16 tfix = 0x0000ffff*t;

            // Next, normalize t a second time so that the angular arc
            // in the range _astart to (_astart + _asweep) maps to unit
            // interval [0,1]. Pixels that are oriented at normalized
            // angles above or below this range are colored according
            // to the caller-specified EXTEND flag and SPREAD_METHOD.
            tfix *= _tmult;
            int n = tfix >> 16;

            COLOR color = 0;
            if (n == 0 || _extend != 0)
            {
                if (_spread == SPREAD_PAD && n != 0)
                    color = _cstops->GetPadColor(_extend, opacity);
                else
                {
                    tfix &= 0x0000ffff;  // isolate fraction
                    if (_extend < 0)
                        tfix ^= 0x0000ffff;

                    if (_spread == SPREAD_REFLECT && (n & 1))
                        tfix ^= 0x0000ffff;

                    color = _cstops->GetColorValue(tfix, opacity);
                }
            }
            outBuf[i] = color;
        }
        xp += 1.0f;
    }
}

// Called by a renderer to create a new conic-gradient object
ConicGradient* CreateConicGradient(float x0, float y0, float astart, float asweep,
                                   SPREAD_METHOD spread, int flags,
                                   const float xform[6])
{
    ConicGrad *grad = new ConicGrad(x0, y0, astart, asweep, spread, flags, xform);
    if (grad == 0 || grad->GetStatus() == false)
    {
        assert(grad != 0 && grad->GetStatus() == true);
        delete grad;
        return 0;  // constructor failed
    }
    return grad;  // success
}

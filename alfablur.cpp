/*
  Copyright (C) 2023-2024 Jerry R. VanAken

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
//  alfablur.cpp:
//    This file contains the implementation of the AlphaBlur class
//    declared in demo.h. For a code example that uses AlphaBlur, see
//    "Compositing and Filtering with Layers" in the user's guide.
//
//---------------------------------------------------------------------

#include <math.h>
#include <string.h>
#include <assert.h>
#include "demo.h"

//---------------------------------------------------------------------
//
// An AlphaBlur object uses a Gaussian filter to blur images, and can
// used, for example, to render drop shadows. The BlurImage function
// blurs only the alpha fields in the source image. The RGB fields in
// the source image are ignored and replaced with a solid color. This
// color can be customized if the caller prefers not to use the
// default color, which is black with 8-bit alpha = 127. The user can
// create a custom filter kernel by specifying the filter width
// (always an odd integer) and standard deviation, or simply accept
// the defaults for these parameters.
//
//---------------------------------------------------------------------

// Public constructor
AlphaBlur::AlphaBlur(const PIXEL_BUFFER *srcimage,
                     int kwidth, float stddev, COLOR color) :
             _kwidth(0), _stddev(0), _numpixels(0), _index(0)
{
    memset(&_blurbuf, 0, sizeof(_blurbuf));
    if (CreateFilterKernel(kwidth, stddev) == false)
    {
        assert(_kwidth > 0);
        return;  // failed to create filter kernel
    }
    _rgba = color;
    _alpha = color >> 24;
    _rgb = color & 0x00ffffff;
    if (BlurImage(srcimage) == false)
    {
        _kwidth = 0;
        assert(_kwidth);
        return;  // failed to generate blurred image
    }
    // If the blurred image was successfully created, _kwidth > 0
    assert(_kwidth > 0);
}

AlphaBlur::~AlphaBlur()
{
    DeleteRawPixels(_blurbuf.pixels);
    delete[] _kcoeff;
}

// Public function: Retrieves filter parameters and blur color.
// Returns true if the blurred image was successfully created.
bool AlphaBlur::GetBlurParams(int *kwidth, float *stddev, COLOR *color)
{
    if (_kwidth == 0)
        return false;  // fail - blurred image not created

    if (stddev)
        *stddev = _stddev;
    if (kwidth)
        *kwidth = _kwidth;
    if (color)
        *color = _rgba;
    return true;  // success
}

// Public function: Calculates the bounding box needed to contain the
// blurred version of the input image. (Blurring expands the image.)
// The 'bbox' parameter specifies the bounding box for the input
// image. The blurred image's bounding box is written to the 'blurbox'
// structure. The dimensions of the blurred image are obtained by
// extending all four sides of the input image's bounding box outward
// by rad = floor(kwidth/2) pixels, where kwidth is the kernel width.
// Returns true if (1) the blurred image was successfully created and
// (2) the width and height specified in 'bbox' match those in the
// input image supplied to the AlphaBlur constructor.
//
bool AlphaBlur::GetBlurredBoundingBox(SGRect *blurbbox, const SGRect *bbox)
{
    int rad = _kwidth/2;

    if (_kwidth == 0)
        return false;  // fail - blurred image not created

    if ((_blurbuf.width != bbox->w + 2*rad) ||
        (_blurbuf.height != bbox->h + 2*rad))
    {
        assert(_blurbuf.width == bbox->w + 2*rad);
        assert(_blurbuf.height == bbox->h + 2*rad);
        return false;  // fail - invalid input parameters
    }
    blurbbox->x = bbox->x - rad;
    blurbbox->y = bbox->y - rad;
    blurbbox->w = bbox->w + 2*rad;
    blurbbox->h = bbox->h + 2*rad;
    return true;  // success
}

// Private function: Calculates the coefficients for the Gaussian
// filter kernel. Due to symmetry, only the coefficients on the right
// side (and center) of the kernel need to be explicitly calculated
// and stored in the kcoeff array. Note that this array's length is
// 1+kwidth/2, not kwidth! The coefficients are stored as 16-bit
// integer alpha values in the range 0 to 1.0, where 1.0 is
// represented as 0x0000ffff. Returns true if successful.
//
bool AlphaBlur::CreateFilterKernel(int kwidth, float stddev)
{
    if (kwidth < 1 && stddev <= 0)  // use defaults?
    {
        stddev = 3.0f;
        kwidth = 3*stddev;
    }
    else if (stddev <= 0)
    {
        stddev = kwidth/3.0f;
    }
    else if (kwidth < 1)
    {
        kwidth = 3*stddev;
    }
    kwidth |= 1;  // kernel width must be odd integer
    int rad = kwidth/2;
    float *ktemp = new float[rad+1];
    if (ktemp == 0)
    {
        assert(ktemp);
        return false;  // fail - out of memory
    }
    float denom = 2*stddev*stddev;
    float sum = ktemp[0] = 1.0f;
    for (int i = 1; i <= rad; ++i)
    {
        ktemp[i] = exp(-(i*i)/denom);
        sum += 2*ktemp[i];
    }
    _kcoeff = new COLOR[rad+1];
    if (_kcoeff == 0)
    {
        assert(_kcoeff);
        return false;  // fail - out of memory
    }
    float norm = 0x0000ffff/sum;
    for (int i = 0; i <= rad; ++i)
        _kcoeff[i] = norm*ktemp[i];  // normalize

    _stddev = stddev;
    _kwidth = kwidth;
    delete[] ktemp;
    return true;  // success
}

// Private function: Convolves a 1-D source image 'src' with a
// Gaussian filter kernel of width 'kwidth', and writes the result to
// destination image 'dst'. The 'src' array is of length 'len', and
// the 'dst' array has length len+2*rad, where rad = floor(kwidth/2).
// Kernel width 'kwidth' is always odd. This function assumes that
// the caller previously set the 2*rad pixels on either side of the
// 'len' pixels in the 'src' array to zero.
//
void AlphaBlur::ApplyGaussianFilter(COLOR dst[], const COLOR src[], int len)
{
    int rad = _kwidth/2;
    const COLOR *psrc = &src[-rad], *pR, *pL;
    COLOR *pdst = &dst[0], *pK, sum;

    len += _kwidth;
    for (int i = 0; i < len; ++i)
    {
        pR = pL = psrc++;
        pK = &_kcoeff[0];
        sum = (pK[0]*pR[0]) >> 16;
        for (int j = 0; j < rad; ++j)
            sum += (*++pK * (*++pR + *--pL)) >> 16;

        *pdst++ = sum;
    }
}

// Private function: Uses a Gaussian filter to blur the 32-bpp input
// image in the pixel buffer specified by input parameter 'srcimage'.
// The function allocates an output buffer and writes the blurred
// image to this buffer. Blurring will expand the rectangular image
// by rad = 2*floor(kwidth/2) pixels on each of its four sides. Only
// the alpha channel in the source image is filtered; the source RGB
// values are ignored, and the specified fill color (in the '_color'
// member) is used instead to color the resulting blurred image.
//
bool AlphaBlur::BlurImage(const PIXEL_BUFFER *srcimage)
{
    if (srcimage->pixels == 0 || srcimage->width < 1 || srcimage->height < 1 ||
        srcimage->pitch < srcimage->width || srcimage->depth != 32)
    {
        assert(0);
        return false;  // fail - invalid input image descriptor
    }

    // Allocate pixel buffer to store blurred image
    int rad = _kwidth/2;
    _blurbuf.width = srcimage->width + 2*rad;
    _blurbuf.height = srcimage->height + 2*rad;
    _blurbuf.pitch = _blurbuf.width*sizeof(COLOR);
    _blurbuf.depth = srcimage->depth;
    _numpixels = _blurbuf.width*_blurbuf.height;
    _blurbuf.pixels = new COLOR[_numpixels];
    if (_blurbuf.pixels == 0)
    {
        assert(_blurbuf.pixels);
        _numpixels = _blurbuf.width = _blurbuf.height = 0;
        return false;  // fail - out of memory
    }
    COLOR *pincol = &srcimage->pixels[0];
    COLOR *poutcol = &_blurbuf.pixels[rad];
    int instride = srcimage->pitch/sizeof(srcimage->pixels[0]);
    int outstride = _blurbuf.width;

    // Allocate scratch buffer to filter columns from input image.
    int scratchwidth = srcimage->height + 4*rad;
    COLOR *scratch = AllocateRawPixels(scratchwidth, 2, RGBA(0,0,0,0));

    // First, process the image in the vertical direction by
    // convolving each column with a 1-D filter. Blurring will
    // increase the height of each column by 2*rad pixels. Each
    // for-loop iteration below vertically filters one column
    // of pixels.
    for (int i = 0; i < srcimage->width; ++i)
    {
        // Copy the next column of pixels from the input image to
        // the first row of the scratch buffer. Leave margins of
        // 2*rad pixels (set to zero) on either side of the copied
        // pixels so we don't have to deal with boundary conditions.
        // Only the alpha fields of the copied pixels are preserved.
        // To improve filtering precision, each 8-bit alpha value is
        // converted to a 16-bit alpha value.
        COLOR *pin = pincol++;
        COLOR *pdst = &scratch[2*rad];
        for (int j = 0; j < srcimage->height; ++j)
        {
            COLOR alpha = *pin >> 24;
            *pdst++ = alpha | (alpha << 8);
            pin = &pin[instride];
        }

        // Convolve the input pixels in the scratch buffer's first
        // row with the 1-D Gaussian filter. Write the blurred result
        // to the second row of the scratch buffer.
        COLOR *psrc = &scratch[2*rad];
        pdst = &scratch[rad + scratchwidth];
        ApplyGaussianFilter(pdst, psrc, srcimage->height);
        psrc = pdst;

        // Copy the blurred pixels from the second row of the scratch
        // buffer to the next column of the blurred image buffer
        COLOR *pout = poutcol++;
        for (int j = 0; j < _blurbuf.height; ++j)
            *pout = *psrc++, pout = &pout[outstride];
    }
    DeleteRawPixels(scratch);

    // So far, vertical filtering has increased the height of the
    // image by 2*rad. Next, this intermediate image will be filtered
    // horizontally, which will increase the image width by 2*rad.
    COLOR *pinrow = &_blurbuf.pixels[rad];
    COLOR *poutrow = &_blurbuf.pixels[0];

    // Allocate scratch buffer to filter rows from intermediate image
    scratchwidth = srcimage->width + 4*rad;
    scratch = AllocateRawPixels(scratchwidth, 2, RGBA(0,0,0,0));

    // To construct the final output image, each for-loop iteration
    // below horizontally filters one row from the intermediate image
    for (int j = 0; j < _blurbuf.height; ++j)
    {
        // Copy the next row of pixels from the intermediate image
        // to the first row of the scratch buffer, and leave
        // margins of 2*rad pixels (set to zero) on either side.
        COLOR *pin = pinrow;
        COLOR *pdst = &scratch[2*rad];
        pinrow = &pinrow[outstride];
        for (int i = 0; i < srcimage->width; ++i)
            *pdst++ = *pin++;

        // Convolve input pixels with Gaussian filter. The blurred
        // result resides in the second row of the scratch buffer.
        COLOR *psrc = &scratch[2*rad];
        pdst = &scratch[rad + scratchwidth];
        ApplyGaussianFilter(pdst, psrc, srcimage->width);
        psrc = pdst;

        // Combine the 16-bit alpha values from the current row of the
        // blurred image with the 8-bit alpha field in the fill color
        COLOR *pout = poutrow;
        poutrow = &poutrow[outstride];
        if (_alpha == 255)
        {
            // The fill color is fully opaque, so just truncate the
            // 16-bit alpha to 8 bits and plug it into the fill color
            for (int i = 0; i < _blurbuf.width; ++i)
            {
                COLOR ablur = *psrc++ >> 8;
                *pout++ = (ablur << 24) | _rgb;
            }
        }
        else
        {
            for (int i = 0; i < _blurbuf.width; ++i)
            {
                COLOR ablur16 = *psrc++;
                COLOR ablur8 = ablur16 >> 8;
                if (ablur8 == 0)
                {
                    *pout++ = 0;
                }
                else if (ablur8 == 255)
                {
                    *pout++ = _rgba;
                }
                else
                {
                    // Multiply the 8-bit alpha from the fill color
                    // by the 16-bit alpha from the blurred image
                    int alpha = _alpha*ablur16;
                    alpha += 0x00008000;
                    alpha >>= 16;
                    *pout++ = (alpha << 24) | _rgb;
                }
            }
        }
    }
    DeleteRawPixels(scratch);
    return true;
}

// Public function: Retrieves a description of the blurred image
// buffer. Returns true if creation of the blurred image succeeded.
//
bool AlphaBlur::GetBlurredImage(PIXEL_BUFFER *blurbuf)
{
    *blurbuf = _blurbuf;
    return (_kwidth != 0);
}

// Public function: Implements the ImageReader interface,
// which is declared in renderer.h and is used by the
// EnhancedRenderer::SetPattern function
//
int AlphaBlur::ReadPixels(COLOR *buffer, int count)
{
    if (_numpixels == 0 || _index >= _numpixels)
        return 0;

    if (count > (_numpixels - _index))
        count = _numpixels - _index;

    COLOR *ptr = &_blurbuf.pixels[_index];

    for (int i = 0; i < count; ++i)
        *buffer++ = *ptr++;

    _index += count;
    return count;
}

// Public function: Restores the ReadPixels function's data
// pointer to the starting pixel in the blurred image
//
bool AlphaBlur::RewindData()
{
    _index = 0;
    return true;
}

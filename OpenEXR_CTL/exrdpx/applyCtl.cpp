///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2013 Academy of Motion Picture Arts and Sciences 
// ("A.M.P.A.S."). Portions contributed by others as indicated.
// All rights reserved.
// 
// A worldwide, royalty-free, non-exclusive right to copy, modify, create
// derivatives, and use, in source and binary forms, is hereby granted, 
// subject to acceptance of this license. Performance of any of the 
// aforementioned acts indicates acceptance to be bound by the following 
// terms and conditions:
//
//  * Copies of source code, in whole or in part, must retain the 
//    above copyright notice, this list of conditions and the 
//    Disclaimer of Warranty.
//
//  * Use in binary form must retain the above copyright notice, 
//    this list of conditions and the Disclaimer of Warranty in the
//    documentation and/or other materials provided with the distribution.
//
//  * Nothing in this license shall be deemed to grant any rights to 
//    trademarks, copyrights, patents, trade secrets or any other 
//    intellectual property of A.M.P.A.S. or any contributors, except 
//    as expressly stated herein.
//
//  * Neither the name "A.M.P.A.S." nor the name of any other 
//    contributors to this software may be used to endorse or promote 
//    products derivative of or based on this software without express 
//    prior written permission of A.M.P.A.S. or the contributors, as 
//    appropriate.
// 
// This license shall be construed pursuant to the laws of the State of 
// California, and any disputes related thereto shall be subject to the 
// jurisdiction of the courts therein.
//
// Disclaimer of Warranty: THIS SOFTWARE IS PROVIDED BY A.M.P.A.S. AND 
// CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, 
// BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS 
// FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT ARE DISCLAIMED. IN NO 
// EVENT SHALL A.M.P.A.S., OR ANY CONTRIBUTORS OR DISTRIBUTORS, BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, RESITUTIONARY, 
// OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
// THE POSSIBILITY OF SUCH DAMAGE.
//
// WITHOUT LIMITING THE GENERALITY OF THE FOREGOING, THE ACADEMY 
// SPECIFICALLY DISCLAIMS ANY REPRESENTATIONS OR WARRANTIES WHATSOEVER 
// RELATED TO PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS IN THE ACADEMY 
// COLOR ENCODING SYSTEM, OR APPLICATIONS THEREOF, HELD BY PARTIES OTHER 
// THAN A.M.P.A.S., WHETHER DISCLOSED OR UNDISCLOSED.
///////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//
//	Apply CTL transforms
//
//-----------------------------------------------------------------------------

#include <applyCtl.h>

#include <ImfCtlApplyTransforms.h>
#include <CtlSimdInterpreter.h>
#include <ImfStandardAttributes.h>
#include <ImfHeader.h>
#include <ImfFrameBuffer.h>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

using namespace std;
using namespace Ctl;
using namespace Imf;
using namespace Imath;


namespace {

void
initializeExrFrameBuffer
    (int w,
     int h,
     const Array2D<Rgba> &pixels,
     FrameBuffer &fb)
{
    fb.insert ("R", Slice (HALF,				// type
			   (char *)(&pixels[0][0].r),		// base
			   sizeof (pixels[0][0]),		// xStride
			   sizeof (pixels[0][0]) * w));		// yStride

    fb.insert ("G", Slice (HALF,				// type
			   (char *)(&pixels[0][0].g),		// base
			   sizeof (pixels[0][0]),		// xStride
			   sizeof (pixels[0][0]) * w));		// yStride

    fb.insert ("B", Slice (HALF,				// type
			   (char *)(&pixels[0][0].b),		// base
			   sizeof (pixels[0][0]),		// xStride
			   sizeof (pixels[0][0]) * w));		// yStride
}


void
initializeDpxFrameBuffer
    (int w,
     int h,
     const Array2D<Rgba> &pixels,
     FrameBuffer &fb)
{
    fb.insert ("DR_film", Slice (HALF,				// type
			      (char *)(&pixels[0][0].r),	// base
			      sizeof (pixels[0][0]),		// xStride
			      sizeof (pixels[0][0]) * w));	// yStride

    fb.insert ("DG_film", Slice (HALF,				// type
			      (char *)(&pixels[0][0].g),	// base
			      sizeof (pixels[0][0]),		// xStride
			      sizeof (pixels[0][0]) * w));	// yStride

    fb.insert ("DB_film", Slice (HALF,				// type
			      (char *)(&pixels[0][0].b),	// base
			      sizeof (pixels[0][0]),		// xStride
			      sizeof (pixels[0][0]) * w));	// yStride
}

} // namespace


void
applyCtlExrToDpx (const vector<string> &transformNames,
	          Header exrHeader,
		  const Array2D<Rgba> &exrPixels,
		  int w,
		  int h,
		  Array2D<Rgba> &dpxPixels)
{
    //
    // Make sure that the input header contains information about
    // the OpenEXR input file's chromaticites.
    //

    if (!hasChromaticities (exrHeader))
	addChromaticities (exrHeader, Chromaticities());

    Header envHeader;

    //
    // Set up input and output FrameBuffer objects for the transforms.
    //

    FrameBuffer inFb;
    initializeExrFrameBuffer (w, h, exrPixels, inFb);

    FrameBuffer outFb;
    initializeDpxFrameBuffer (w, h, dpxPixels, outFb);

    //
    // Run the CTL transforms
    //

    Box2i transformWindow (V2i (0, 0), V2i (w - 1, h - 1));

    SimdInterpreter interpreter;

    #ifdef CTL_MODULE_BASE_PATH
	//
	// The configuration scripts has defined a default
	// location for CTL modules.  Include this location
	// in the CTL module search path.
	//

	vector<string> paths = interpreter.modulePaths();
	paths.push_back (CTL_MODULE_BASE_PATH);
	interpreter.setModulePaths (paths);

    #endif

    Header outHeader;
    ImfCtl::applyTransforms (interpreter,
			     transformNames,
			     transformWindow,
			     envHeader,
			     exrHeader,
			     inFb,
			     outHeader,
			     outFb);
}


void
applyCtlDpxToExr (const vector<string> &transformNames,
		  const Array2D<Rgba> &dpxPixels,
		  int w,
		  int h,
		  const Chromaticities &exrChromaticities,
		  Array2D<Rgba> &exrPixels)
{
    //
    // Create a dummy input header.
    // Create an environment header that contains the
    // desired chromaticities for the OpenEXR output file.
    //

    Header inHeader;
    Header envHeader;

    envHeader.insert
	("chromaticities", ChromaticitiesAttribute (exrChromaticities));

    //
    // Set up input and output FrameBuffer objects for the transforms.
    //

    FrameBuffer inFb;
    initializeDpxFrameBuffer (w, h, dpxPixels, inFb);

    FrameBuffer outFb;
    initializeExrFrameBuffer (w, h, exrPixels, outFb);

    //
    // Run the CTL transforms
    //

    Box2i transformWindow (V2i (0, 0), V2i (w - 1, h - 1));

    SimdInterpreter interpreter;

    #ifdef CTL_MODULE_BASE_PATH
	//
	// The configuration scripts has defined a default
	// location for CTL modules.  Include this location
	// in the CTL module search path.
	//

	vector<string> paths = interpreter.modulePaths();
	paths.push_back (CTL_MODULE_BASE_PATH);
	interpreter.setModulePaths (paths);

    #endif

    Header outHeader;

    ImfCtl::applyTransforms (interpreter,
			     transformNames,
			     transformWindow,
			     envHeader,
			     inHeader,
			     inFb,
			     outHeader,
			     outFb);
}

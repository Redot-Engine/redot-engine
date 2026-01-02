/**************************************************************************/
/*  FIRFilter.h                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

////////////////////////////////////////////////////////////////////////////////
///
/// General FIR digital filter routines with MMX optimization.
///
/// Note : MMX optimized functions reside in a separate, platform-specific file,
/// e.g. 'mmx_win.cpp' or 'mmx_gcc.cpp'
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#ifndef FIRFilter_H
#define FIRFilter_H

#include "STTypes.h"
#include <stddef.h>

namespace soundtouch {

class FIRFilter {
protected:
	// Number of FIR filter taps
	uint length;
	// Number of FIR filter taps divided by 8
	uint lengthDiv8;

	// Result divider factor in 2^k format
	uint resultDivFactor;

	// Memory for filter coefficients
	SAMPLETYPE *filterCoeffs;
	SAMPLETYPE *filterCoeffsStereo;

	virtual uint evaluateFilterStereo(SAMPLETYPE *dest,
			const SAMPLETYPE *src,
			uint numSamples) const;
	virtual uint evaluateFilterMono(SAMPLETYPE *dest,
			const SAMPLETYPE *src,
			uint numSamples) const;
	virtual uint evaluateFilterMulti(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples, uint numChannels);

public:
	FIRFilter();
	virtual ~FIRFilter();

	/// Operator 'new' is overloaded so that it automatically creates a suitable instance
	/// depending on if we've a MMX-capable CPU available or not.
	static void *operator new(size_t s);

	static FIRFilter *newInstance();

	/// Applies the filter to the given sequence of samples.
	/// Note : The amount of outputted samples is by value of 'filter_length'
	/// smaller than the amount of input samples.
	///
	/// \return Number of samples copied to 'dest'.
	uint evaluate(SAMPLETYPE *dest,
			const SAMPLETYPE *src,
			uint numSamples,
			uint numChannels);

	uint getLength() const;

	virtual void setCoefficients(const SAMPLETYPE *coeffs,
			uint newLength,
			uint uResultDivFactor);
};

// Optional subclasses that implement CPU-specific optimizations:

#ifdef SOUNDTOUCH_ALLOW_MMX

/// Class that implements MMX optimized functions exclusive for 16bit integer samples type.
class FIRFilterMMX : public FIRFilter {
protected:
	short *filterCoeffsUnalign;
	short *filterCoeffsAlign;

	virtual uint evaluateFilterStereo(short *dest, const short *src, uint numSamples) const override;

public:
	FIRFilterMMX();
	~FIRFilterMMX();

	virtual void setCoefficients(const short *coeffs, uint newLength, uint uResultDivFactor) override;
};

#endif // SOUNDTOUCH_ALLOW_MMX

#ifdef SOUNDTOUCH_ALLOW_SSE
/// Class that implements SSE optimized functions exclusive for floating point samples type.
class FIRFilterSSE : public FIRFilter {
protected:
	float *filterCoeffsUnalign;
	float *filterCoeffsAlign;

	virtual uint evaluateFilterStereo(float *dest, const float *src, uint numSamples) const override;

public:
	FIRFilterSSE();
	~FIRFilterSSE();

	virtual void setCoefficients(const float *coeffs, uint newLength, uint uResultDivFactor) override;
};

#endif // SOUNDTOUCH_ALLOW_SSE

} //namespace soundtouch

#endif // FIRFilter_H

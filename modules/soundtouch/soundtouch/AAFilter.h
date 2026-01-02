/**************************************************************************/
/*  AAFilter.h                                                            */
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
/// Sampled sound tempo changer/time stretch algorithm. Changes the sound tempo
/// while maintaining the original pitch by using a time domain WSOLA-like method
/// with several performance-increasing tweaks.
///
/// Anti-alias filter is used to prevent folding of high frequencies when
/// transposing the sample rate with interpolation.
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

#ifndef AAFilter_H
#define AAFilter_H

#include "FIFOSampleBuffer.h"
#include "STTypes.h"

namespace soundtouch {

class AAFilter {
protected:
	class FIRFilter *pFIR;

	/// Low-pass filter cut-off frequency, negative = invalid
	double cutoffFreq;

	/// num of filter taps
	uint length;

	/// Calculate the FIR coefficients realizing the given cutoff-frequency
	void calculateCoeffs();

public:
	AAFilter(uint length);

	~AAFilter();

	/// Sets new anti-alias filter cut-off edge frequency, scaled to sampling
	/// frequency (nyquist frequency = 0.5). The filter will cut off the
	/// frequencies than that.
	void setCutoffFreq(double newCutoffFreq);

	/// Sets number of FIR filter taps, i.e. ~filter complexity
	void setLength(uint newLength);

	uint getLength() const;

	/// Applies the filter to the given sequence of samples.
	/// Note : The amount of outputted samples is by value of 'filter length'
	/// smaller than the amount of input samples.
	uint evaluate(SAMPLETYPE *dest,
			const SAMPLETYPE *src,
			uint numSamples,
			uint numChannels) const;

	/// Applies the filter to the given src & dest pipes, so that processed amount of
	/// samples get removed from src, and produced amount added to dest
	/// Note : The amount of outputted samples is by value of 'filter length'
	/// smaller than the amount of input samples.
	uint evaluate(FIFOSampleBuffer &dest,
			FIFOSampleBuffer &src) const;
};

} //namespace soundtouch

#endif

/**************************************************************************/
/*  PeakFinder.h                                                          */
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
/// The routine detects highest value on an array of values and calculates the
/// precise peak location as a mass-center of the 'hump' around the peak value.
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

#ifndef _PeakFinder_H_
#define _PeakFinder_H_

namespace soundtouch {

class PeakFinder {
protected:
	/// Min, max allowed peak positions within the data vector
	int minPos, maxPos;

	/// Calculates the mass center between given vector items.
	double calcMassCenter(const float *data, ///< Data vector.
			int firstPos, ///< Index of first vector item belonging to the peak.
			int lastPos ///< Index of last vector item belonging to the peak.
	) const;

	/// Finds the data vector index where the monotoniously decreasing signal crosses the
	/// given level.
	int findCrossingLevel(const float *data, ///< Data vector.
			float level, ///< Goal crossing level.
			int peakpos, ///< Peak position index within the data vector.
			int direction /// Direction where to proceed from the peak: 1 = right, -1 = left.
	) const;

	// Finds real 'top' of a peak hump from neighnourhood of the given 'peakpos'.
	int findTop(const float *data, int peakpos) const;

	/// Finds the 'ground' level, i.e. smallest level between two neighboring peaks, to right-
	/// or left-hand side of the given peak position.
	int findGround(const float *data, /// Data vector.
			int peakpos, /// Peak position index within the data vector.
			int direction /// Direction where to proceed from the peak: 1 = right, -1 = left.
	) const;

	/// get exact center of peak near given position by calculating local mass of center
	double getPeakCenter(const float *data, int peakpos) const;

public:
	/// Constructor.
	PeakFinder();

	/// Detect exact peak position of the data vector by finding the largest peak 'hump'
	/// and calculating the mass-center location of the peak hump.
	///
	/// \return The location of the largest base harmonic peak hump.
	double detectPeak(const float *data, /// Data vector to be analyzed. The data vector has
										 /// to be at least 'maxPos' items long.
			int minPos, ///< Min allowed peak location within the vector data.
			int maxPos ///< Max allowed peak location within the vector data.
	);
};

} //namespace soundtouch

#endif // _PeakFinder_H_

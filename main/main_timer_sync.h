/**************************************************************************/
/*  main_timer_sync.h                                                     */
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

/**
 * @file main_timer_sync.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "core/config/engine.h"

// Uncomment this define to get more debugging logs for the delta smoothing.
// #define GODOT_DEBUG_DELTA_SMOOTHER

struct MainFrameTime {
	double process_step; ///< delta time to advance during process()
	int physics_steps; ///< number of times to iterate the physics engine
	double interpolation_fraction; ///< fraction through the current physics tick

	void clamp_process_step(double min_process_step, double max_process_step);
};

class MainTimerSync {
	class DeltaSmoother {
	public:
		/// pass the recorded delta, returns a smoothed delta
		int64_t smooth_delta(int64_t p_delta);

	private:
		/// The calling code should prevent 0 or negative values of delta
		/// (preventing divide by zero)
		void update_refresh_rate_estimator(int64_t p_delta);
		bool fps_allows_smoothing(int64_t p_delta);

		/// Estimated vsync delta (monitor refresh rate)
		int64_t _vsync_delta = 16666;

		/// Keep track of accumulated time so we know how many vsyncs to advance by
		int64_t _leftover_time = 0;

		/// Keep a rough measurement of the FPS as we run.
		/// If this drifts a long way below or above the refresh rate, the machine
		/// is struggling to keep up, and we can switch off smoothing. This
		/// also deals with the case that the user has overridden the vsync in the GPU settings,
		/// in which case we don't want to try smoothing.
		static const int MEASURE_FPS_OVER_NUM_FRAMES = 64;

		int64_t _measurement_time = 0;
		int64_t _measurement_frame_count = 0;
		int64_t _measurement_end_frame = MEASURE_FPS_OVER_NUM_FRAMES;
		int64_t _measurement_start_time = 0;
		bool _measurement_allows_smoothing = true;

		/// We can estimate the fps by growing it on condition
		/// that a large proportion of frames are higher than the current estimate.
		int32_t _estimated_fps = 0;
		int32_t _hits_at_estimated = 0;
		int32_t _hits_above_estimated = 0;
		int32_t _hits_below_estimated = 0;
		int32_t _hits_one_above_estimated = 0;
		int32_t _hits_one_below_estimated = 0;
		bool _estimate_complete = false;
		bool _estimate_locked = false;

		/// @name Data for averaging the delta over a second or so to prevent spurious values
		/// @{
		int64_t _estimator_total_delta = 0;
		int32_t _estimator_delta_readings = 0;
		/// @}

		void made_new_estimate() {
			_hits_above_estimated = 0;
			_hits_at_estimated = 0;
			_hits_below_estimated = 0;
			_hits_one_above_estimated = 0;
			_hits_one_below_estimated = 0;

			_estimate_complete = false;

#ifdef GODOT_DEBUG_DELTA_SMOOTHER
			print_line("estimated fps " + itos(_estimated_fps));
#endif
		}

	} _delta_smoother;

	/// @name Wall clock time measured on the main thread
	/// @{
	uint64_t last_cpu_ticks_usec = 0;
	uint64_t current_cpu_ticks_usec = 0;
	/// @}

	/// Logical game time since last physics timestep
	double time_accum = 0;

	/// Current difference between wall clock time and reported sum of process_steps
	double time_deficit = 0;

	/// Number of frames back for keeping accumulated physics steps roughly constant.
	/// value of 12 chosen because that is what is required to make 144 Hz monitors
	/// behave well with 60 Hz physics updates. The only worse commonly available refresh
	/// would be 85, requiring CONTROL_STEPS = 17.
	static const int CONTROL_STEPS = 12;

	/// Sum of physics steps done over the last (i+1) frames
	int accumulated_physics_steps[CONTROL_STEPS];

	/// Typical value for accumulated_physics_steps[i] is either this or this plus one
	int typical_physics_steps[CONTROL_STEPS];

	int fixed_fps = 0;

protected:
	/// @return The fraction of p_physics_step required for the timer to overshoot
	/// before advance_core considers changing the physics_steps return from
	/// the typical values as defined by typical_physics_steps
	double get_physics_jitter_fix();

	/// Gets our best bet for the average number of physics steps per render frame.
	/// @return Number of frames back this data is consistent
	int get_average_physics_steps(double &p_min, double &p_max);

	/// Advance physics clock by p_process_step.
	/// @return Appropriate number of steps to simulate
	MainFrameTime advance_core(double p_physics_step, int p_physics_ticks_per_second, double p_process_step);

	/// Calls advance_core, keeps track of deficit it adds to animaption_step, make sure the deficit sum stays close to zero
	MainFrameTime advance_checked(double p_physics_step, int p_physics_ticks_per_second, double p_process_step);

	/// Determine wall clock step since last iteration
	double get_cpu_process_step();

public:
	MainTimerSync();

	/// Start the clock
	void init(uint64_t p_cpu_ticks_usec);
	/// Set measured wall clock time
	void set_cpu_ticks_usec(uint64_t p_cpu_ticks_usec);
	/// Set fixed fps
	void set_fixed_fps(int p_fixed_fps);

	/// Advance one physics frame.
	/// @return timesteps to take
	MainFrameTime advance(double p_physics_step, int p_physics_ticks_per_second);
};

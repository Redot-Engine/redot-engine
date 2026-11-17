// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_COMMAND_PARAMS_H
#define PX_VEHICLE_COMMAND_PARAMS_H

#include "foundation/PxFoundation.h"
#include "vehicle/PxVehicleLimits.h"
#include "vehicle/PxVehicleParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Each command value may be associated with a table specifying a normalized response as a function of longitudinal speed.
Multiple instances of PxVehicleCommandValueResponseTable allow a normalized response to be authored as a multi-variate 
piecewise polynomial with normalized command response expressed as a nonlinear function of command value and speed.
*/
struct PxVehicleCommandValueResponseTable
{
	enum Enum
	{
		eMAX_NB_SPEED_RESPONSES = 64
	};

	/**
	\brief The command value associated with the table of speed responses.
	*/
	PxReal commandValue;

	/**
	\brief A lookup table specifying the normalised response to the specified command value as a function of longitudinal speed.
	\note Each entry in the speedResponses table must be of the form (speed, normalizedResponse).
	\note The longitudinal speeds in the table must form a strictly increasing series.
	\note The normalized responses must be in range [0, 1].
	*/
	PxVehicleFixedSizeLookupTable<PxReal, eMAX_NB_SPEED_RESPONSES> speedResponses;
};

/**
\note Brake, drive and steer response typically reduce at increased longitudinal speed. Moreover, response to a brake, throttle or steer command is typically
nonlinear and may be subject to dead zones where response is constant with either zero or non-zero response. PxVehicleCommandNonLinearResponseParams allows
command responses to be authored as multi-variate piecewise polynomials with normalized command response a function of command value and longitudinal speed.
*/
class PxVehicleCommandNonLinearResponseParams
{
public:

	enum Enum
	{
		eMAX_NB_COMMAND_VALUES = 8
	};

	PxVehicleCommandNonLinearResponseParams()
		: nbSpeedResponses(0),
		  nbCommandValues(0)
	{
	}

	void clear()
	{
		nbCommandValues = 0;
		nbSpeedResponses = 0;
	}

	/**
	\brief Add a table of normalised response vs speed and associated it with a specified command value. 
	\note commandValueSpeedResponses must be authored as a series of strictly increasing speeds with form {speed, normalizedResponse}
	\note The responses added must form a series of strictly increasing command values.
	*/
	bool addResponse(const PxVehicleCommandValueResponseTable& commandValueSpeedResponses)
	{
		const PxReal commandValue = commandValueSpeedResponses.commandValue;
		const PxReal* speeds = commandValueSpeedResponses.speedResponses.xVals;
		const PxReal* responses = commandValueSpeedResponses.speedResponses.yVals;
		const PxU16 nb = PxU16(commandValueSpeedResponses.speedResponses.nbDataPairs);

		PX_CHECK_AND_RETURN_VAL(commandValue >= 0.0f && commandValue <= 1.0f, "PxVehicleCommandAndResponseTable::commandValue must be in range [0, 1]", false);

		PX_CHECK_AND_RETURN_VAL(nbCommandValues < eMAX_NB_COMMAND_VALUES, "PxVehicleNonLinearCommandResponse::addResponse - exceeded maximum number of command responses", false);

		PX_CHECK_AND_RETURN_VAL(((nbSpeedResponses + nb) <= PxVehicleCommandValueResponseTable::eMAX_NB_SPEED_RESPONSES), "PxVehicleNonLinearCommandResponse::addResponse - exceeded maximum number of command responses", false);

		PX_CHECK_AND_RETURN_VAL((0 == nbCommandValues) || (commandValue > commandValues[nbCommandValues - 1]), "PxVehicleNonLinearCommandResponse::addResponse - command must be part of a strictly increasing series", false);

		PX_CHECK_AND_RETURN_VAL(nb > 0, "PxVehicleNonLinearCommandResponse::addResponse - each command response must have at least 1 point", false);

#if PX_CHECKED
		for (PxU32 i = 1; i < nb; i++)
		{
			PX_CHECK_AND_RETURN_VAL(speeds[i] > speeds[i - 1], "PxVehicleNonLinearCommandResponse::addResponse - speeds array must be a strictly increasing series", false);
			PX_CHECK_AND_RETURN_VAL(responses[i] >= 0.0f && responses[i] <= 1.0f , "PxVehicleNonLinearCommandResponse::addResponse - response must be in range [0, 1]", false);
		}
#endif

		commandValues[nbCommandValues] = commandValue;
		nbSpeedResponsesPerCommandValue[nbCommandValues] = nb;
		speedResponsesPerCommandValue[nbCommandValues] = nbSpeedResponses;
		PxMemCopy(speedResponses + 2 * nbSpeedResponses, speeds, sizeof(PxReal)*nb);
		PxMemCopy(speedResponses + 2 * nbSpeedResponses + nb, responses, sizeof(PxReal)*nb);
		nbCommandValues++;
		nbSpeedResponses += nb;
		return true;
	}

public:

	/**
	\brief A ragged array of speeds and normalized responses.
	*/
	PxReal speedResponses[PxVehicleCommandValueResponseTable::eMAX_NB_SPEED_RESPONSES * 2];

	/**
	\brief The number of speeds and normalized responses.
	*/
	PxU16 nbSpeedResponses;

	/**
	\brief The table of speed responses for the ith command value begins at speedResponses[2*speedResponsesPerCommandValue[i]]
	*/
	PxU16 speedResponsesPerCommandValue[eMAX_NB_COMMAND_VALUES];

	/**
	\brief The ith command value has N speed responses with N = nbSpeedRenponsesPerCommandValue[i].
	*/
	PxU16 nbSpeedResponsesPerCommandValue[eMAX_NB_COMMAND_VALUES];		

	/**
	\brief The command values.
	*/
	PxReal commandValues[eMAX_NB_COMMAND_VALUES];

	/**
	\brief The number of command values.
	*/
	PxU16 nbCommandValues;
};

/**
\brief A description of the per wheel response to an input command.
*/
struct PxVehicleCommandResponseParams
{
	/**
	\brief A nonlinear response to command value expressed as a lookup table of normalized response as a function of command value and longitudinal speed.
	\note The effect of the default state of nonlinearResponse is a linear response to command value that is independent of longitudinal speed.
	*/
	PxVehicleCommandNonLinearResponseParams nonlinearResponse;

	/**
	\brief A description of the per wheel response multiplier to an input command.
	*/
	PxReal wheelResponseMultipliers[PxVehicleLimits::eMAX_NB_WHEELS];	

	/**
	\brief The maximum response that occurs when the wheel response multiplier has value 1.0 and nonlinearResponse is in the default state of linear response.
	*/
	PxF32 maxResponse;
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

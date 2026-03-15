// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LeverCycleTrackable.generated.h"

/**
 *  Optional interface for ISwitchable actors whose response to SetActivated()
 *  takes time to complete (e.g. a platform travelling to its destination).
 *
 *  ALever polls IsLeverCycleComplete() each tick while a cycle is running.
 *  When every implementing linked target returns true, the lever unlocks.
 *
 *  Actors that do NOT implement this interface are treated as instantly
 *  complete — their SetActivated() is assumed to be a synchronous call.
 *
 *  Only implement this on actors that genuinely need the lever to wait.
 */
UINTERFACE(MinimalAPI)
class ULeverCycleTrackable : public UInterface
{
	GENERATED_BODY()
};

class ILeverCycleTrackable
{
	GENERATED_BODY()

public:

	/**
	 * Returns true when the target has settled into a stable resting state
	 * after the last SetActivated() call and no longer needs to block the lever.
	 *
	 * For a moving platform: true when MoveProgress is 0 (Base) or 1 (Activated),
	 * false while in transit.
	 *
	 * The default returns true so un-overridden actors never block the lever.
	 */
	virtual bool IsLeverCycleComplete() const { return true; }
};

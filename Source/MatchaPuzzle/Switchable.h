// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Switchable.generated.h"

/**
 *  Shared activation contract for any actor that can be turned on or off by
 *  an external trigger (AFloorSwitch, ALever, etc.).
 *
 *  Implement this interface on any gameplay actor — AMovingPlatform,
 *  ARotatingPlatform, a door, a light — and assign it to a switch's
 *  LinkedTargets array without any tight coupling between the two sides.
 *
 *  The default SetActivated body is intentionally empty so that partial
 *  implementations (actors that react to only one direction) compile cleanly.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USwitchable : public UInterface
{
	GENERATED_BODY()
};

class ISwitchable
{
	GENERATED_BODY()

public:

	/**
	 * Activate or deactivate this object.
	 * Implementors decide what "active" means for their specific type.
	 */
	virtual void SetActivated(bool bActivate) {}

	/**
	 * Activate or deactivate this object with an optional direction modifier.
	 *
	 * DirectionModifier scales the object's internal movement offset:
	 *   1.0  = normal direction   (e.g. platform moves toward +SwitchOffset)
	 *  -1.0  = inverted direction (e.g. platform moves toward -SwitchOffset)
	 *
	 * The default implementation ignores the modifier and delegates to
	 * SetActivated, so all existing implementors remain fully compatible
	 * without any changes on their side.
	 */
	virtual void SetActivatedWithDirection(bool bActivate, float DirectionModifier)
	{
		SetActivated(bActivate);
	}
};

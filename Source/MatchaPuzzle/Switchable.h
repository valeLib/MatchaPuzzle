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
};

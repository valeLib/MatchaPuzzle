// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ActivatableTarget.generated.h"

/**
 *  Timed-activation contract for any actor that can be turned ON and OFF by
 *  a lever (or any other external trigger that manages its own timing).
 *
 *  This interface is intentionally separate from ISwitchable:
 *   - ISwitchable  : binary on/off, no timing, for platforms driven directly
 *                    by the lever state (on while held, off when released).
 *   - IActivatableTarget : timed activation — the lever schedules Activate()
 *                    after a configurable delay, then Deactivate() after a
 *                    configurable duration.  The target itself has no concept
 *                    of timing and just responds to each individual call.
 *
 *  ── Implementing in Blueprint ────────────────────────────────────────────
 *   1. Open the Blueprint's Class Settings.
 *   2. Under "Interfaces", add "ActivatableTarget".
 *   3. Implement the "Activate" and "Deactivate" events that now appear in
 *      the My Blueprint panel.  Wire up whatever logic you want (lights,
 *      doors, particles, spawners…).
 *
 *  ── Implementing in C++ ──────────────────────────────────────────────────
 *   class AMyActor : public AActor, public IActivatableTarget
 *   {
 *       GENERATED_BODY()
 *   public:
 *       virtual void Activate_Implementation()   override;
 *       virtual void Deactivate_Implementation() override;
 *   };
 *
 *  Note: always override the _Implementation variants in C++.  The lever
 *  calls IActivatableTarget::Execute_Activate / Execute_Deactivate, which
 *  dispatch correctly to both C++ and Blueprint overrides.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UActivatableTarget : public UInterface
{
	GENERATED_BODY()
};

class IActivatableTarget
{
	GENERATED_BODY()

public:

	/**
	 * Called when the lever's ActivateDelay timer fires.
	 * The target should turn itself ON here (enable emissive, open a door, etc.).
	 * The default C++ body is a deliberate no-op so that actors may opt into
	 * only one direction if desired.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Activatable")
	void Activate();

	/**
	 * Called either when ActiveDuration expires or when the lever deactivates
	 * before the duration completes.
	 * The target should turn itself OFF here.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Activatable")
	void Deactivate();
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ClawControllable.generated.h"

/**
 *  Directional-input contract for any actor that can be driven by
 *  AClawMachineLever.
 *
 *  Horizontal and Vertical values use the lever's configured min/max ranges,
 *  so the implementing actor does not need to know about those limits.
 *
 *  ── Implementing in Blueprint ────────────────────────────────────────────
 *   1. Open the Blueprint's Class Settings.
 *   2. Under "Interfaces", add "ClawControllable".
 *   3. Implement the "SetLeverInput" event in the My Blueprint panel.
 *      Map Horizontal / Vertical to whatever movement you need.
 *
 *  ── Implementing in C++ ──────────────────────────────────────────────────
 *   class AMyPlatform : public AActor, public IClawControllable
 *   {
 *       GENERATED_BODY()
 *   public:
 *       virtual void SetLeverInput_Implementation(float H, float V) override;
 *   };
 *
 *  Note: always override the _Implementation variant in C++.
 *  AClawMachineLever calls IClawControllable::Execute_SetLeverInput, which
 *  dispatches correctly to both C++ and Blueprint overrides.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UClawControllable : public UInterface
{
	GENERATED_BODY()
};

class IClawControllable
{
	GENERATED_BODY()

public:

	/**
	 * Receives the current lever position whenever it changes.
	 *
	 * @param Horizontal  Current horizontal value within [HorizontalMin, HorizontalMax]
	 *                    — negative is left, positive is right relative to the lever.
	 * @param Vertical    Current vertical (down) value within [VerticalMin, VerticalMax]
	 *                    — VerticalMin is fully up, VerticalMax is fully pressed down.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Claw Lever")
	void SetLeverInput(float Horizontal, float Vertical);
};

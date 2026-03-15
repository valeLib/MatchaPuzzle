// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Switchable.h"
#include "ClawControllable.h"
#include "MovingPlatform.generated.h"

class UStaticMeshComponent;

/** Axis along which the platform oscillates (Automatic mode only) */
UENUM(BlueprintType)
enum class EMovementAxis : uint8
{
	LeftRight   UMETA(DisplayName = "Left / Right (Y)"),
	UpDown      UMETA(DisplayName = "Up / Down (Z)"),
	FrontBack   UMETA(DisplayName = "Front / Back (X)")
};

/** Determines how the platform is driven at runtime */
UENUM(BlueprintType)
enum class EPlatformControlMode : uint8
{
	Automatic        UMETA(DisplayName = "Automatic (oscillation)"),
	SwitchControlled UMETA(DisplayName = "Switch Controlled"),
	LeverControlled  UMETA(DisplayName = "Lever Controlled (claw machine)")
};

/**
 *  A platform that can either oscillate continuously (Automatic), be driven
 *  between two positions by an external floor switch (SwitchControlled), or
 *  follow continuous directional input from an AClawMachineLever (LeverControlled).
 *
 *  SwitchControlled behaviour:
 *   SetActivated(true)  — platform moves to   Activated = StartLocation + SwitchOffset.
 *   SetActivated(false) — platform returns to  Base      = StartLocation.
 *   Both directions are unconditional and fire-and-forget.
 *
 *  LeverControlled behaviour:
 *   AClawMachineLever calls SetLeverInput(H, V) whenever its values change.
 *   Each Tick the platform position is recomputed from StartLocation using the
 *   stored values, so there is no drift regardless of frame rate:
 *
 *   NewLocation = StartLocation
 *               + Normalize(LeverHorizontalDirection) * LeverHorizontalScale * H
 *               + Normalize(LeverVerticalDirection)   * LeverVerticalScale   * V
 */
UCLASS()
class AMovingPlatform : public AActor, public ISwitchable, public IClawControllable
{
	GENERATED_BODY()

	/** Visual / collision mesh for the platform */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* PlatformMesh;

public:

	AMovingPlatform();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Activates or deactivates this platform.
	 * Only takes effect when ControlMode == SwitchControlled.
	 * Intended to be called by AFloorSwitch or any other external actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Platform|Switch")
	virtual void SetActivated(bool bActivate) override;

	/**
	 * Receives continuous directional input from AClawMachineLever.
	 * Only takes effect when ControlMode == LeverControlled.
	 * Stores the values; the actual position update runs in TickLeverControlled.
	 *
	 * @param Horizontal  Value within the lever's [HorizontalMin, HorizontalMax]
	 * @param Vertical    Value within the lever's [VerticalMin, VerticalMax]
	 */
	virtual void SetLeverInput_Implementation(float Horizontal, float Vertical) override;

protected:

	// ── Mode selection ────────────────────────────────────────────────────────

	/** Choose how this platform is driven */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform")
	EPlatformControlMode ControlMode = EPlatformControlMode::Automatic;

	// ── Automatic mode parameters (original, unchanged) ───────────────────────

	/** Which axis the platform moves along */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Automatic",
		meta = (EditCondition = "ControlMode == EPlatformControlMode::Automatic",
			EditConditionHides))
	EMovementAxis MovementAxis = EMovementAxis::LeftRight;

	/** Total distance the platform travels from its start position (units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Automatic",
		meta = (ClampMin = "0",
			EditCondition = "ControlMode == EPlatformControlMode::Automatic",
			EditConditionHides))
	float MoveDistance = 100.f;

	/** Speed of the platform (units per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Automatic",
		meta = (ClampMin = "0.01",
			EditCondition = "ControlMode == EPlatformControlMode::Automatic",
			EditConditionHides))
	float MoveSpeed = 100.f;

	// ── Switch-controlled mode parameters ────────────────────────────────────

	/**
	 * World-space offset from the platform's start position to its activated
	 * (target) position.  Example: (0, 0, 200) raises the platform 200 units.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Switch",
		meta = (EditCondition = "ControlMode == EPlatformControlMode::SwitchControlled",
			EditConditionHides))
	FVector SwitchOffset = FVector(0.f, 0.f, 200.f);

	/**
	 * Travel speed in world units per second.
	 * The actual travel time is derived automatically from SwitchOffset.Size(),
	 * so changing the offset never requires a manual time adjustment:
	 *
	 *   TravelTime = SwitchOffset.Size() / SwitchMoveSpeed
	 *
	 * Example: SwitchOffset = (0, 0, 300), SwitchMoveSpeed = 150 → 2 s.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Switch",
		meta = (ClampMin = "1.0",
			EditCondition = "ControlMode == EPlatformControlMode::SwitchControlled",
			EditConditionHides))
	float SwitchMoveSpeed = 200.f;

	// ── Lever-controlled mode parameters ─────────────────────────────────────

	/**
	 * World-space direction the platform moves when horizontal lever input is
	 * positive (E key).  The vector is normalised at runtime, so only its
	 * direction matters — use LeverHorizontalScale for magnitude.
	 * Example: (0, 1, 0) moves the platform along the world Y axis.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Lever",
		meta = (EditCondition = "ControlMode == EPlatformControlMode::LeverControlled",
			EditConditionHides))
	FVector LeverHorizontalDirection = FVector(0.f, 1.f, 0.f);

	/**
	 * Maximum displacement (units) along LeverHorizontalDirection when the
	 * horizontal lever is at its maximum value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Lever",
		meta = (EditCondition = "ControlMode == EPlatformControlMode::LeverControlled",
			EditConditionHides))
	float LeverHorizontalScale = 200.f;

	/**
	 * World-space direction the platform moves when vertical lever input is
	 * positive (A key / down press).  Normalised at runtime.
	 * Example: (0, 0, -1) lowers the platform when the lever is pushed down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Lever",
		meta = (EditCondition = "ControlMode == EPlatformControlMode::LeverControlled",
			EditConditionHides))
	FVector LeverVerticalDirection = FVector(0.f, 0.f, -1.f);

	/**
	 * Maximum displacement (units) along LeverVerticalDirection when the
	 * vertical lever is at its maximum value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Lever",
		meta = (EditCondition = "ControlMode == EPlatformControlMode::LeverControlled",
			EditConditionHides))
	float LeverVerticalScale = 200.f;

private:

	// ── Shared state ──────────────────────────────────────────────────────────

	/** Location recorded at BeginPlay — origin for all movement modes */
	FVector StartLocation;

	// ── Automatic mode state ──────────────────────────────────────────────────

	/** Running time accumulator driving the ping-pong motion (Automatic only) */
	float RunningTime = 0.f;

	// ── Switch-controlled mode state ──────────────────────────────────────────

	/** Whether the platform is currently activated by a switch */
	bool bIsActivated = false;

	/**
	 * Normalised movement progress: 0 = base position, 1 = target position.
	 * Advanced or rewound each tick based on bIsActivated.
	 */
	float MoveProgress = 0.f;

	// ── Lever-controlled mode state ───────────────────────────────────────────

	/** Last horizontal value received from the lever. Set by SetLeverInput_Implementation. */
	float LeverHorizontalInput = 0.f;

	/** Last vertical value received from the lever. Set by SetLeverInput_Implementation. */
	float LeverVerticalInput = 0.f;

	// ── Private tick helpers ──────────────────────────────────────────────────

	/** Contains the original automatic oscillation logic, completely unchanged */
	void TickAutomatic(float DeltaTime);

	/** Contains the switch-driven interpolation logic */
	void TickSwitchControlled(float DeltaTime);

	/**
	 * Recomputes the platform position from StartLocation using the last stored
	 * lever input values.  Called every frame in LeverControlled mode.
	 * Position is always derived from StartLocation, never accumulated, so there
	 * is no drift regardless of how long the lever has been held.
	 */
	void TickLeverControlled();

	/**
	 * After each teleport move, finds any pawns whose capsule now overlaps the
	 * platform mesh and pushes them out using the physics MTD (Minimum Translation
	 * Distance).  The displacement is forced to the horizontal plane so a
	 * descending platform never drives a character through the floor.
	 */
	void DisplaceOverlappingPawns();
};

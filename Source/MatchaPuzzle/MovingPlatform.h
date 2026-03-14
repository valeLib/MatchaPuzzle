// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Switchable.h"
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
	SwitchControlled UMETA(DisplayName = "Switch Controlled")
};

/**
 *  A platform that can either oscillate continuously (Automatic) or be driven
 *  between two positions by an external floor switch (SwitchControlled).
 *
 *  Automatic behaviour is identical to the original implementation and is
 *  completely unaffected by the new mode.  SwitchControlled behaviour is
 *  triggered via SetActivated().
 */
UCLASS()
class AMovingPlatform : public AActor, public ISwitchable
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
	 * Time in seconds to travel from the base to the target position (and back).
	 * Must be greater than zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Switch",
		meta = (ClampMin = "0.01",
			EditCondition = "ControlMode == EPlatformControlMode::SwitchControlled",
			EditConditionHides))
	float MoveDuration = 1.f;

	/**
	 * When true the platform returns to its base position once the switch is
	 * released.  When false it holds at the target position until explicitly
	 * deactivated by a one-shot or second trigger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform|Switch",
		meta = (EditCondition = "ControlMode == EPlatformControlMode::SwitchControlled",
			EditConditionHides))
	bool bReturnWhenReleased = true;

private:

	// ── Shared state ──────────────────────────────────────────────────────────

	/** Location recorded at BeginPlay — origin for both movement modes */
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

	// ── Private tick helpers ──────────────────────────────────────────────────

	/** Contains the original automatic oscillation logic, completely unchanged */
	void TickAutomatic(float DeltaTime);

	/** Contains the switch-driven interpolation logic */
	void TickSwitchControlled(float DeltaTime);

	/**
	 * After each teleport move, finds any pawns whose capsule now overlaps the
	 * platform mesh and pushes them out using the physics MTD (Minimum Translation
	 * Distance).  The displacement is forced to the horizontal plane so a
	 * descending platform never drives a character through the floor.
	 */
	void DisplaceOverlappingPawns();
};

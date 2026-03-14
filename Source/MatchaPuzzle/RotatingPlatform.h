// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Switchable.h"
#include "RotatingPlatform.generated.h"

class UStaticMeshComponent;

/** Axis around which the platform rotates */
UENUM(BlueprintType)
enum class ERotationAxis : uint8
{
	Yaw     UMETA(DisplayName = "Yaw (Z)"),
	Pitch   UMETA(DisplayName = "Pitch (Y)"),
	Roll    UMETA(DisplayName = "Roll (X)")
};

/** Determines how the platform is driven at runtime */
UENUM(BlueprintType)
enum class ERotatingControlMode : uint8
{
	Automatic        UMETA(DisplayName = "Automatic (continuous spin)"),
	SwitchControlled UMETA(DisplayName = "Switch Controlled")
};

/**
 *  A platform that can either spin continuously (Automatic) or rotate by a
 *  fixed angle when triggered by an external switch (SwitchControlled).
 *
 *  Automatic behaviour is identical to the original implementation and is
 *  completely unaffected by the new mode.  SwitchControlled behaviour is
 *  triggered via SetActivated() (ISwitchable).
 */
UCLASS()
class ARotatingPlatform : public AActor, public ISwitchable
{
	GENERATED_BODY()

	/** Visual / collision mesh for the platform */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* PlatformMesh;

public:

	ARotatingPlatform();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * ISwitchable — activate or deactivate this platform.
	 * Only has an effect when ControlMode == SwitchControlled.
	 * Intended to be called by AFloorSwitch, ALever, or any other trigger actor.
	 */
	virtual void SetActivated(bool bActivate) override;

protected:

	// ── Mode selection ────────────────────────────────────────────────────────

	/** Choose how this platform is driven */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	ERotatingControlMode ControlMode = ERotatingControlMode::Automatic;

	// ── Shared ────────────────────────────────────────────────────────────────

	/** Axis around which the platform rotates (used by both modes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	ERotationAxis RotationAxis = ERotationAxis::Yaw;

	// ── Automatic mode parameters ─────────────────────────────────────────────

	/** Continuous spin speed in degrees per second (Automatic only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Automatic",
		meta = (EditCondition = "ControlMode == ERotatingControlMode::Automatic",
			EditConditionHides))
	float RotationSpeed = 90.f;

	// ── Switch-controlled mode parameters ─────────────────────────────────────

	/**
	 * Angle in degrees to rotate from the platform's start rotation when activated.
	 * Positive values follow the right-hand rule for the chosen axis.
	 * Example: Yaw = 90 rotates the platform 90° counter-clockwise (top view).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Switch",
		meta = (EditCondition = "ControlMode == ERotatingControlMode::SwitchControlled",
			EditConditionHides))
	float SwitchRotationAngle = 90.f;

	/**
	 * Time in seconds to travel from the start rotation to the target rotation
	 * (and back).  Must be greater than zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Switch",
		meta = (ClampMin = "0.01",
			EditCondition = "ControlMode == ERotatingControlMode::SwitchControlled",
			EditConditionHides))
	float RotationDuration = 1.f;

	/**
	 * When true the platform returns to its start rotation once the switch is
	 * released.  When false it holds at the target rotation until explicitly
	 * deactivated by a one-shot or second trigger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Switch",
		meta = (EditCondition = "ControlMode == ERotatingControlMode::SwitchControlled",
			EditConditionHides))
	bool bReturnWhenReleased = true;

private:

	// ── Shared state ──────────────────────────────────────────────────────────

	/** World rotation recorded at BeginPlay — origin for SwitchControlled mode */
	FRotator StartRotation;

	// ── Switch-controlled mode state ──────────────────────────────────────────

	/** Whether the platform is currently activated by a switch */
	bool bIsActivated = false;

	/**
	 * Normalised rotation progress: 0 = start rotation, 1 = target rotation.
	 * Advanced or rewound each tick based on bIsActivated.
	 */
	float RotationProgress = 0.f;

	// ── Private tick helpers ──────────────────────────────────────────────────

	/** Original continuous-spin logic, completely unchanged */
	void TickAutomatic(float DeltaTime);

	/** Switch-driven angular interpolation */
	void TickSwitchControlled(float DeltaTime);
};

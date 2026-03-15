// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Switchable.h"
#include "LeverCycleTrackable.h"
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
 *  Internal phase of the SwitchControlled rotation cycle.
 *  Single source of truth for motion state — replaces the old bool combination.
 *
 *  Idle            At base rotation (RotationProgress = 0).  Ready.
 *  Advancing       Moving from base toward target (0 → 1).
 *  AtTarget        At target (RotationProgress = 1).  No auto-return.
 *  HoldingAtTarget At target.  Waiting HoldAtTargetDuration before returning.
 *  Rewinding       Moving from target back to base (1 → 0).
 */
enum class ERotationCyclePhase : uint8
{
	Idle,
	Advancing,
	AtTarget,
	HoldingAtTarget,
	Rewinding
};

/**
 *  A platform that can either spin continuously (Automatic) or rotate by a
 *  fixed angle when triggered by an external switch (SwitchControlled).
 *
 *  SwitchControlled mode — four behaviour combinations:
 *
 *  ReturnWhenReleased=false, ReturnWhenDone=false
 *    One-way: rotates to target and stays permanently.
 *
 *  ReturnWhenReleased=true,  ReturnWhenDone=false
 *    Hold: external SetActivated(false) returns the platform to base.
 *    Designed for hold-style switches; not cycle-lever–compatible.
 *
 *  ReturnWhenReleased=false, ReturnWhenDone=true
 *    Cycle: auto-returns after HoldAtTargetDuration.  Lever waits for
 *    the full round-trip before unlocking.
 *
 *  ReturnWhenReleased=true,  ReturnWhenDone=true
 *    Combined: auto-return fires on arrival.  An external SetActivated(false)
 *    can also trigger an early return mid-transit.  Both paths lead to the
 *    same Rewinding phase; whichever fires first wins.
 *
 *  ILeverCycleTrackable
 *    IsLeverCycleComplete() returns true only when the platform is Idle or
 *    AtTarget — i.e. settled and not blocking the lever's cycle.
 */
UCLASS()
class ARotatingPlatform : public AActor, public ISwitchable, public ILeverCycleTrackable
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

	/** Clears the hold timer so no callback fires after the actor is torn down. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * ISwitchable — activate or deactivate this platform.
	 * Only has an effect when ControlMode == SwitchControlled.
	 * Intended to be called by AFloorSwitch, ALever, or any other trigger actor.
	 */
	virtual void SetActivated(bool bActivate) override;

	/**
	 * ILeverCycleTrackable — reports whether this platform is at a stable
	 * resting state and no longer blocking the lever's cycle.
	 *
	 * Returns true when Phase is Idle (at base) or AtTarget (at target, no
	 * auto-return).  Returns false while Advancing, HoldingAtTarget, or Rewinding.
	 */
	virtual bool IsLeverCycleComplete() const override;

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
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Switch",
		meta = (EditCondition = "ControlMode == ERotatingControlMode::SwitchControlled",
			EditConditionHides))
	float SwitchRotationAngle = 90.f;

	/**
	 * Time in seconds to travel between base and target (both directions).
	 * Must be greater than zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Switch",
		meta = (ClampMin = "0.01",
			EditCondition = "ControlMode == ERotatingControlMode::SwitchControlled",
			EditConditionHides))
	float RotationDuration = 1.f;

	/**
	 * When true, an external SetActivated(false) command returns the platform to
	 * base.  Designed for hold-style switches.  Also allows early cancellation
	 * when combined with ReturnWhenDone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Switch",
		meta = (EditCondition = "ControlMode == ERotatingControlMode::SwitchControlled",
			EditConditionHides))
	bool bReturnWhenReleased = true;

	/**
	 * When true, the platform automatically returns to base once it reaches the
	 * target rotation, after waiting HoldAtTargetDuration seconds.
	 * No external SetActivated(false) is needed.  Designed for cycle-based levers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Switch",
		meta = (EditCondition = "ControlMode == ERotatingControlMode::SwitchControlled",
			EditConditionHides))
	bool bReturnWhenDone = false;

	/**
	 * Seconds the platform holds at the target rotation before returning to base.
	 * 0 = return immediately upon arrival.
	 * Only relevant when ReturnWhenDone = true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation|Switch",
		meta = (ClampMin = "0",
			EditCondition = "ControlMode == ERotatingControlMode::SwitchControlled && bReturnWhenDone",
			EditConditionHides))
	float HoldAtTargetDuration = 0.f;

private:

	// ── Shared state ──────────────────────────────────────────────────────────

	/** World rotation recorded at BeginPlay — origin for SwitchControlled mode */
	FRotator StartRotation;

	// ── Switch-controlled mode state ──────────────────────────────────────────

	/**
	 * Current motion phase.  The single source of truth for what the platform
	 * is doing; drives TickSwitchControlled and IsLeverCycleComplete.
	 */
	ERotationCyclePhase Phase = ERotationCyclePhase::Idle;

	/**
	 * Normalised rotation progress: 0 = base rotation, 1 = target rotation.
	 * Advanced (Advancing) or rewound (Rewinding) each tick.
	 */
	float RotationProgress = 0.f;

	/**
	 * One-shot timer that fires after HoldAtTargetDuration when Phase ==
	 * HoldingAtTarget.  Transitions Phase to Rewinding when it expires.
	 * Cleared in EndPlay and whenever the platform leaves the holding phase.
	 */
	FTimerHandle HoldTimer;

	// ── Private helpers ───────────────────────────────────────────────────────

	/** Original continuous-spin logic, completely unchanged */
	void TickAutomatic(float DeltaTime);

	/** Switch-driven angular interpolation with full cycle-phase management */
	void TickSwitchControlled(float DeltaTime);

	/**
	 * Called when RotationProgress reaches 1 (target rotation).
	 * Transitions to HoldingAtTarget (ReturnWhenDone=true) or AtTarget (false).
	 * Schedules the hold timer when HoldAtTargetDuration > 0.
	 */
	void OnReachedTarget();

	/**
	 * Timer callback: fires after HoldAtTargetDuration expires.
	 * Transitions Phase to Rewinding so the platform begins returning.
	 */
	void OnHoldTimerExpired();

	/**
	 * Applies the world rotation derived from the current RotationProgress.
	 * Called every frame while Advancing or Rewinding.
	 */
	void ApplyRotation();
};

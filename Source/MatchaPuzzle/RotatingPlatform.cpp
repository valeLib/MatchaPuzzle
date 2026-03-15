// Copyright Epic Games, Inc. All Rights Reserved.

#include "RotatingPlatform.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

ARotatingPlatform::ARotatingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create the mesh as root — gives the platform both visuals and collision
	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	RootComponent = PlatformMesh;

	PlatformMesh->SetMobility(EComponentMobility::Movable);
}

void ARotatingPlatform::BeginPlay()
{
	Super::BeginPlay();

	// Record world rotation as origin for SwitchControlled mode
	StartRotation = GetActorRotation();
}

void ARotatingPlatform::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Cancel the hold timer so no callback fires on a dangling pointer during level unload.
	GetWorldTimerManager().ClearTimer(HoldTimer);
}

void ARotatingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (ControlMode)
	{
	case ERotatingControlMode::Automatic:
		TickAutomatic(DeltaTime);
		break;

	case ERotatingControlMode::SwitchControlled:
		TickSwitchControlled(DeltaTime);
		break;
	}
}

// ── ISwitchable ────────────────────────────────────────────────────────────────

void ARotatingPlatform::SetActivated(bool bActivate)
{
	// Guard: this API is only meaningful in SwitchControlled mode
	if (ControlMode != ERotatingControlMode::SwitchControlled)
	{
		return;
	}

	if (bActivate)
	{
		// Start or resume advancing toward the target rotation.
		switch (Phase)
		{
		case ERotationCyclePhase::Idle:
		case ERotationCyclePhase::Rewinding:
			// Begin moving from wherever we are toward the target.
			Phase = ERotationCyclePhase::Advancing;
			break;

		case ERotationCyclePhase::HoldingAtTarget:
			// External call while holding — caller wants the platform to stay.
			// Cancel the pending auto-return and settle at target.
			GetWorldTimerManager().ClearTimer(HoldTimer);
			Phase = ERotationCyclePhase::AtTarget;
			break;

		default:
			// Advancing or AtTarget — already heading to or at the target; no change.
			break;
		}
	}
	else if (bReturnWhenReleased)
	{
		// Return to base on external release command (hold-switch semantic).
		switch (Phase)
		{
		case ERotationCyclePhase::Advancing:
		case ERotationCyclePhase::AtTarget:
			Phase = ERotationCyclePhase::Rewinding;
			break;

		case ERotationCyclePhase::HoldingAtTarget:
			// Early return wins over the pending auto-return timer.
			GetWorldTimerManager().ClearTimer(HoldTimer);
			Phase = ERotationCyclePhase::Rewinding;
			break;

		default:
			// Idle or already Rewinding — nothing to do.
			break;
		}
	}
}

// ── ILeverCycleTrackable ───────────────────────────────────────────────────────

bool ARotatingPlatform::IsLeverCycleComplete() const
{
	// The lever can unlock when the platform is settled at either endpoint:
	//   Idle    — returned to base (full round-trip done for cycle levers)
	//   AtTarget — permanently at target (one-way or hold-style levers)
	// All transit and hold phases return false — the lever must keep waiting.
	return Phase == ERotationCyclePhase::Idle
		|| Phase == ERotationCyclePhase::AtTarget;
}

// ── Private tick helpers ───────────────────────────────────────────────────────

void ARotatingPlatform::TickAutomatic(float DeltaTime)
{
	// Original implementation — not modified in any way.

	if (FMath::IsNearlyZero(RotationSpeed))
	{
		return;
	}

	const float DeltaDegrees = RotationSpeed * DeltaTime;

	FRotator DeltaRotation = FRotator::ZeroRotator;
	switch (RotationAxis)
	{
	case ERotationAxis::Yaw:   DeltaRotation.Yaw   = DeltaDegrees; break;
	case ERotationAxis::Pitch: DeltaRotation.Pitch = DeltaDegrees; break;
	case ERotationAxis::Roll:  DeltaRotation.Roll  = DeltaDegrees; break;
	}

	AddActorLocalRotation(DeltaRotation);
}

void ARotatingPlatform::TickSwitchControlled(float DeltaTime)
{
	if (RotationDuration <= 0.f)
	{
		return;
	}

	// Only Advancing and Rewinding produce movement; all other phases are inert.
	switch (Phase)
	{
	case ERotationCyclePhase::Advancing:
	{
		const float ProgressRate = DeltaTime / RotationDuration;
		RotationProgress = FMath::Clamp(RotationProgress + ProgressRate, 0.f, 1.f);
		ApplyRotation();

		if (RotationProgress >= 1.f)
		{
			OnReachedTarget();
		}
		break;
	}

	case ERotationCyclePhase::Rewinding:
	{
		const float ProgressRate = DeltaTime / RotationDuration;
		RotationProgress = FMath::Clamp(RotationProgress - ProgressRate, 0.f, 1.f);
		ApplyRotation();

		if (RotationProgress <= 0.f)
		{
			Phase = ERotationCyclePhase::Idle;
		}
		break;
	}

	default:
		// Idle, AtTarget, HoldingAtTarget — no movement this frame.
		break;
	}
}

void ARotatingPlatform::OnReachedTarget()
{
	if (bReturnWhenDone)
	{
		if (HoldAtTargetDuration > 0.f)
		{
			// Pause at the target for the configured duration, then rewind.
			Phase = ERotationCyclePhase::HoldingAtTarget;

			FTimerDelegate Del;
			Del.BindUObject(this, &ARotatingPlatform::OnHoldTimerExpired);
			GetWorldTimerManager().SetTimer(HoldTimer, Del, HoldAtTargetDuration, /*bLoop=*/ false);
		}
		else
		{
			// Zero hold duration — begin rewinding immediately on arrival.
			Phase = ERotationCyclePhase::Rewinding;
		}
	}
	else
	{
		// No auto-return — settle permanently at the target rotation.
		Phase = ERotationCyclePhase::AtTarget;
	}
}

void ARotatingPlatform::OnHoldTimerExpired()
{
	// Only act if we are still in the holding phase.
	// SetActivated(false) could have already moved us to Rewinding or AtTarget.
	if (Phase == ERotationCyclePhase::HoldingAtTarget)
	{
		Phase = ERotationCyclePhase::Rewinding;
	}
}

void ARotatingPlatform::ApplyRotation()
{
	// SmoothStep maps normalised progress to an S-curve so both directions
	// ease in and out, matching MovingPlatform's SwitchControlled feel.
	const float SmoothedProgress = FMath::SmoothStep(0.f, 1.f, RotationProgress);
	const float Angle = SwitchRotationAngle * SmoothedProgress;

	FRotator DeltaRot = FRotator::ZeroRotator;
	switch (RotationAxis)
	{
	case ERotationAxis::Yaw:   DeltaRot.Yaw   = Angle; break;
	case ERotationAxis::Pitch: DeltaRot.Pitch = Angle; break;
	case ERotationAxis::Roll:  DeltaRot.Roll  = Angle; break;
	}

	SetActorRotation(StartRotation + DeltaRot);
}

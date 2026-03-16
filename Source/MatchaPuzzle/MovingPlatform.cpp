// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovingPlatform.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

AMovingPlatform::AMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create the mesh as root — gives the platform both visuals and collision
	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	RootComponent = PlatformMesh;

	PlatformMesh->SetMobility(EComponentMobility::Movable);
}

void AMovingPlatform::BeginPlay()
{
	Super::BeginPlay();

	// Record world position as origin for all movement modes
	StartLocation = GetActorLocation();
}

void AMovingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (ControlMode)
	{
	case EPlatformControlMode::Automatic:
		TickAutomatic(DeltaTime);
		break;

	case EPlatformControlMode::SwitchControlled:
		TickSwitchControlled(DeltaTime);
		break;

	case EPlatformControlMode::LeverControlled:
		TickLeverControlled();
		break;
	}
}

void AMovingPlatform::SetActivated(bool bActivate)
{
	// Guard: this API is only meaningful in SwitchControlled mode
	if (ControlMode != EPlatformControlMode::SwitchControlled)
	{
		return;
	}

	bIsActivated = bActivate;
}

bool AMovingPlatform::IsLeverCycleComplete() const
{
	if (ControlMode != EPlatformControlMode::SwitchControlled)
	{
		// Automatic and LeverControlled modes have no discrete cycle endpoint.
		return true;
	}

	// In transit: MoveProgress is strictly between 0 and 1.
	// Complete when settled at Base (0) or Activated (1).
	return MoveProgress <= 0.f || MoveProgress >= 1.f;
}

void AMovingPlatform::SetLeverInput_Implementation(float Horizontal, float Vertical)
{
	// Guard: this API is only meaningful in LeverControlled mode
	if (ControlMode != EPlatformControlMode::LeverControlled)
	{
		return;
	}

	LeverHorizontalInput = Horizontal;
	LeverVerticalInput   = Vertical;
}

// ── Private tick helpers ───────────────────────────────────────────────────────

void AMovingPlatform::TickAutomatic(float DeltaTime)
{
	// Original implementation — not modified in any way.

	if (MoveDistance <= 0.f || MoveSpeed <= 0.f)
	{
		return;
	}

	// Advance time based on speed and distance so one full
	// ping-pong cycle = 2 * MoveDistance / MoveSpeed seconds
	RunningTime += DeltaTime * MoveSpeed;

	// PingPong oscillates between 0 and MoveDistance
	const float ModTime = FMath::Fmod(RunningTime, MoveDistance * 2.f);
	const float Offset = (ModTime <= MoveDistance) ? ModTime : (MoveDistance * 2.f - ModTime);

	// Build direction vector from the chosen axis
	FVector Direction = FVector::ZeroVector;
	switch (MovementAxis)
	{
	case EMovementAxis::LeftRight:
		Direction = FVector::YAxisVector;   // Y
		break;
	case EMovementAxis::UpDown:
		Direction = FVector::ZAxisVector;   // Z
		break;
	case EMovementAxis::FrontBack:
		Direction = FVector::XAxisVector;   // X
		break;
	}

	SetActorLocation(StartLocation + Direction * Offset);
	DisplaceOverlappingPawns();
}

void AMovingPlatform::TickSwitchControlled(float DeltaTime)
{
	// A zero-length offset or zero speed means there is nowhere to go.
	const float Distance = SwitchOffset.Size();
	if (Distance < KINDA_SMALL_NUMBER || SwitchMoveSpeed <= 0.f)
	{
		return;
	}

	// SetActivated(true)  → Activated = StartLocation + SwitchOffset (advance to 1)
	// SetActivated(false) → Base      = StartLocation               (rewind  to 0)
	// Both transitions are unconditional; the platform always completes its journey.
	const bool bShouldAdvance = bIsActivated  && MoveProgress < 1.f;
	const bool bShouldRewind  = !bIsActivated && MoveProgress > 0.f;

	if (!bShouldAdvance && !bShouldRewind)
	{
		return;
	}

	// Convert world-space speed (units/sec) to a normalised progress rate.
	// Dividing by Distance means one unit of MoveProgress always corresponds to
	// exactly one unit of world travel, so SwitchMoveSpeed is consistent across
	// platforms with different SwitchOffset magnitudes.
	//
	//   TravelTime  = Distance / SwitchMoveSpeed          (seconds, implicit)
	//   ProgressRate = SwitchMoveSpeed / Distance * DeltaTime   (per frame)
	const float ProgressRate = (SwitchMoveSpeed / Distance) * DeltaTime;

	if (bShouldAdvance)
	{
		MoveProgress = FMath::Clamp(MoveProgress + ProgressRate, 0.f, 1.f);
	}
	else
	{
		MoveProgress = FMath::Clamp(MoveProgress - ProgressRate, 0.f, 1.f);
	}

	// SmoothStep maps normalised progress to a smooth S-curve (ease-in / ease-out)
	// that still reaches exactly 0 and 1 at the endpoints — deterministic at any
	// frame rate because MoveProgress accumulates in normalised space.
	const float SmoothedProgress = FMath::SmoothStep(0.f, 1.f, MoveProgress);

	SetActorLocation(StartLocation + SwitchOffset * SmoothedProgress);
	DisplaceOverlappingPawns();
}

void AMovingPlatform::TickLeverControlled()
{
	// Recompute from the fixed StartLocation every frame.
	// This guarantees no drift: the position is always a pure function of
	// StartLocation and the two input scalars.  Accumulating onto the current
	// location is intentionally avoided.
	//
	// NewLocation = StartLocation
	//             + Normalize(LeverHorizontalDirection) * LeverHorizontalScale * LeverHorizontalInput
	//             + Normalize(LeverVerticalDirection)   * LeverVerticalScale   * LeverVerticalInput
	//
	// GetSafeNormal returns FVector::ZeroVector when the input is near-zero,
	// which makes the corresponding term a no-op rather than a crash.

	const FVector HDir = LeverHorizontalDirection.GetSafeNormal();
	const FVector VDir = LeverVerticalDirection.GetSafeNormal();

	const FVector NewLocation = StartLocation
		+ HDir * LeverHorizontalScale * LeverHorizontalInput
		+ VDir * LeverVerticalScale   * LeverVerticalInput;

	SetActorLocation(NewLocation);
	DisplaceOverlappingPawns();
}

void AMovingPlatform::DisplaceOverlappingPawns()
{
	TArray<AActor*> Overlapping;
	PlatformMesh->GetOverlappingActors(Overlapping, APawn::StaticClass());

	for (AActor* Actor : Overlapping)
	{
		UPrimitiveComponent* PawnRoot = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		if (!PawnRoot)
		{
			continue;
		}

		FMTDResult MTD;
		if (!PlatformMesh->ComputePenetration(
			MTD, PawnRoot->GetCollisionShape(),
			Actor->GetActorLocation(), Actor->GetActorQuat()))
		{
			continue;
		}

		// Force the push to be horizontal only.
		// Without this, a platform descending onto the character would push them
		// straight down through the floor instead of out to the side.
		FVector PushDir = MTD.Direction;
		PushDir.Z = 0.f;

		if (PushDir.IsNearlyZero())
		{
			// Character is directly below the platform centre — pick the nearest
			// horizontal direction away from it.
			PushDir = (Actor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (PushDir.IsNearlyZero())
			{
				PushDir = FVector::ForwardVector;
			}
		}
		else
		{
			PushDir.Normalize();
		}

		// 1 cm extra margin keeps the capsule from immediately re-overlapping
		// on the next physics query.
		Actor->SetActorLocation(
			Actor->GetActorLocation() + PushDir * (MTD.Distance + 1.f),
			false, nullptr, ETeleportType::TeleportPhysics);
	}
}

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

	// Record world position as origin for both movement modes
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
	if (MoveDuration <= 0.f)
	{
		return;
	}

	// Determine whether the platform still needs to move this frame.
	// This avoids calling SetActorLocation every frame once the platform is at rest.
	const bool bShouldAdvance = bIsActivated && MoveProgress < 1.f;
	const bool bShouldRewind  = !bIsActivated && bReturnWhenReleased && MoveProgress > 0.f;

	if (!bShouldAdvance && !bShouldRewind)
	{
		return;
	}

	// Drive progress forward or backward at a constant rate
	const float ProgressRate = DeltaTime / MoveDuration;

	if (bShouldAdvance)
	{
		MoveProgress = FMath::Clamp(MoveProgress + ProgressRate, 0.f, 1.f);
	}
	else
	{
		MoveProgress = FMath::Clamp(MoveProgress - ProgressRate, 0.f, 1.f);
	}

	// SmoothStep gives an ease-in / ease-out feel without requiring a curve asset.
	// It maps the linear progress to a smooth S-curve that still reaches exactly
	// 0 and 1 at the endpoints, keeping the motion deterministic.
	const float SmoothedProgress = FMath::SmoothStep(0.f, 1.f, MoveProgress);

	SetActorLocation(StartLocation + SwitchOffset * SmoothedProgress);
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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "RotatingPlatform.h"
#include "Components/StaticMeshComponent.h"

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

void ARotatingPlatform::SetActivated(bool bActivate)
{
	// Guard: this API is only meaningful in SwitchControlled mode
	if (ControlMode != ERotatingControlMode::SwitchControlled)
	{
		return;
	}

	bIsActivated = bActivate;
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
	case ERotationAxis::Yaw:
		DeltaRotation.Yaw = DeltaDegrees;
		break;
	case ERotationAxis::Pitch:
		DeltaRotation.Pitch = DeltaDegrees;
		break;
	case ERotationAxis::Roll:
		DeltaRotation.Roll = DeltaDegrees;
		break;
	}

	AddActorLocalRotation(DeltaRotation);
}

void ARotatingPlatform::TickSwitchControlled(float DeltaTime)
{
	if (RotationDuration <= 0.f)
	{
		return;
	}

	// Determine whether the platform still needs to move this frame.
	const bool bShouldAdvance = bIsActivated && RotationProgress < 1.f;
	const bool bShouldRewind  = !bIsActivated && bReturnWhenReleased && RotationProgress > 0.f;

	if (!bShouldAdvance && !bShouldRewind)
	{
		return;
	}

	// Drive progress forward or backward at a constant rate
	const float ProgressRate = DeltaTime / RotationDuration;

	if (bShouldAdvance)
	{
		RotationProgress = FMath::Clamp(RotationProgress + ProgressRate, 0.f, 1.f);
	}
	else
	{
		RotationProgress = FMath::Clamp(RotationProgress - ProgressRate, 0.f, 1.f);
	}

	// SmoothStep gives an ease-in / ease-out feel, matching MovingPlatform's
	// SwitchControlled transition so both platform types feel consistent.
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

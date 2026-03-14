// Copyright Epic Games, Inc. All Rights Reserved.

#include "FloorSwitch.h"
#include "Switchable.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"

AFloorSwitch::AFloorSwitch()
{
	// Tick is needed only for smooth button animation; kept enabled so
	// bAnimatePress can be toggled at runtime without a restart.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Trigger volume as root so the actor's transform drives the switch position
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	RootComponent = TriggerVolume;

	TriggerVolume->SetBoxExtent(FVector(50.f, 50.f, 25.f));
	TriggerVolume->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	TriggerVolume->SetGenerateOverlapEvents(true);

	// Fixed base — never moves. Assign a mesh in the Details panel.
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(RootComponent);
	BaseMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// Button — moves along local Z to reflect pressed / released state.
	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	ButtonMesh->SetupAttachment(RootComponent);
	ButtonMesh->SetCollisionProfileName(TEXT("NoCollision"));
}

void AFloorSwitch::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap delegates here rather than in the constructor so that the
	// UFunction reflection system is fully initialised before binding.
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(
		this, &AFloorSwitch::OnTriggerBeginOverlap);

	TriggerVolume->OnComponentEndOverlap.AddDynamic(
		this, &AFloorSwitch::OnTriggerEndOverlap);

	// Cache button positions from the editor-placed relative location of ButtonMesh.
	// These are computed once here and never changed again — Tick always interpolates
	// toward one of these two fixed targets, so there is no cumulative drift.
	if (ButtonMesh)
	{
		ReleasedLocation = ButtonMesh->GetRelativeLocation();
		PressedLocation  = ReleasedLocation - FVector(0.f, 0.f, ButtonPressDepth);

		// Create a dynamic material instance for ButtonMesh slot 0 so we can drive
		// the IsPressed scalar parameter at runtime. BaseMesh is never touched.
		if (UMaterialInterface* BaseMat = ButtonMesh->GetMaterial(0))
		{
			ButtonMaterialInstance = UMaterialInstanceDynamic::Create(BaseMat, this);
			ButtonMesh->SetMaterial(0, ButtonMaterialInstance);
		}
	}

	// Sync ButtonMesh position and material to the initial bIsPressed value.
	// If the switch somehow starts pressed, both the mesh offset and the
	// IsPressed parameter will reflect that immediately.
	SetPressedState(bIsPressed);
}

void AFloorSwitch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAnimatePress || !ButtonMesh)
	{
		return;
	}

	// Target is one of the two stable positions computed once in BeginPlay.
	// Reading and writing relative location keeps everything in local space,
	// so the button cannot drift regardless of the actor's world transform.
	const FVector Target  = bIsPressed ? PressedLocation : ReleasedLocation;
	const FVector Current = ButtonMesh->GetRelativeLocation();

	if (!Current.Equals(Target, 0.1f))
	{
		ButtonMesh->SetRelativeLocation(
			FMath::VInterpTo(Current, Target, DeltaSeconds, ButtonMoveSpeed));
	}
}

// ── Overlap callbacks ──────────────────────────────────────────────────────────

void AFloorSwitch::OnTriggerBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor*              OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32                /*OtherBodyIndex*/,
	bool                 /*bFromSweep*/,
	const FHitResult&    /*SweepResult*/)
{
	if (!IsLocalPlayer(OtherActor))
	{
		return;
	}

	// Toggle: each entry flips the platforms between forward and reverse
	if (bToggleable)
	{
		bToggleState = !bToggleState;
		SetTargetsActivated(bToggleState);
		SetPressedState(bToggleState);
		return;
	}

	// One-shot: ignore subsequent overlaps after the first activation
	if (bOneShot && bHasTriggered)
	{
		return;
	}

	bHasTriggered = true;
	SetTargetsActivated(true);
	SetPressedState(true);
}

void AFloorSwitch::OnTriggerEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor*              OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32                /*OtherBodyIndex*/)
{
	if (!IsLocalPlayer(OtherActor))
	{
		return;
	}

	// Toggle and one-shot switches do not respond to the player leaving
	if (bToggleable || bOneShot)
	{
		return;
	}

	SetTargetsActivated(false);
	SetPressedState(false);
}

// ── Helpers ────────────────────────────────────────────────────────────────────

bool AFloorSwitch::IsLocalPlayer(const AActor* OtherActor)
{
	if (!OtherActor)
	{
		return false;
	}

	const APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	return OverlappingPawn && OverlappingPawn->IsPlayerControlled();
}

void AFloorSwitch::SetTargetsActivated(bool bActivate) const
{
	for (AActor* Target : LinkedTargets)
	{
		if (ISwitchable* Switchable = Cast<ISwitchable>(Target))
		{
			Switchable->SetActivated(bActivate);
		}
	}
}

void AFloorSwitch::SetPressedState(bool bPressed)
{
	bIsPressed = bPressed;

	// Update the IsPressed scalar parameter on ButtonMesh's dynamic material.
	// SetScalarParameterValue is a no-op if the parameter name does not exist,
	// so this is safe even if the material does not expose IsPressed.
	if (ButtonMaterialInstance)
	{
		ButtonMaterialInstance->SetScalarParameterValue(
			TEXT("IsPressed"), bIsPressed ? 1.0f : 0.0f);
	}

	if (!bAnimatePress && ButtonMesh)
	{
		// Snap immediately; Tick will not move the button further.
		ButtonMesh->SetRelativeLocation(bIsPressed ? PressedLocation : ReleasedLocation);
	}
	// Animated case: Tick() drives the interpolation each frame toward the
	// stable PressedLocation or ReleasedLocation — no offset is applied here.
}

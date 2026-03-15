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

void AFloorSwitch::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Create or reuse DMIs and push the TileOffset values immediately.
	// This makes every property change in the Details panel visible in the
	// viewport without requiring PIE, because OnConstruction fires on every
	// edit.  At runtime OnConstruction also runs once before BeginPlay, so
	// BeginPlay will find valid DMI pointers already in place.
	InitMaterialInstances();
	ApplyMaterialParameters();

	// Preview the pressed / released state in the viewport based on bStartEnabled.
	// Only the material parameter is driven here — mesh position preview is skipped
	// because ReleasedLocation / PressedLocation are not computed until BeginPlay.
	if (ButtonMaterialInstance)
	{
		ButtonMaterialInstance->SetScalarParameterValue(
			TEXT("IsPressed"), bStartEnabled ? 0.0f : 1.0f);
	}
}

void AFloorSwitch::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the runtime enabled state from the editor property.
	bIsEnabled = bStartEnabled;

	// Bind overlap delegates here rather than in the constructor so that the
	// UFunction reflection system is fully initialised before binding.
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(
		this, &AFloorSwitch::OnTriggerBeginOverlap);

	TriggerVolume->OnComponentEndOverlap.AddDynamic(
		this, &AFloorSwitch::OnTriggerEndOverlap);

	// Cache button positions from the editor-placed relative location of ButtonMesh.
	// These are computed once and never changed — Tick always interpolates toward
	// one of these two fixed targets so there is no cumulative drift.
	// This must stay in BeginPlay: OnConstruction can run at edit time before the
	// actor has a valid world context for physics / gameplay queries.
	if (ButtonMesh)
	{
		ReleasedLocation = ButtonMesh->GetRelativeLocation();
		PressedLocation  = ReleasedLocation - FVector(0.f, 0.f, ButtonPressDepth);
	}

	// Ensure DMIs exist even if OnConstruction was somehow skipped.
	// When OnConstruction has already run (the normal case) this is a no-op
	// because InitMaterialInstances detects and reuses existing instances.
	InitMaterialInstances();
	ApplyMaterialParameters();

	// Sync button position and IsPressed material parameter to the initial state.
	// Pressed = Disabled: a disabled switch starts sunken; an enabled one elevated.
	SetPressedState(!bIsEnabled);
}

void AFloorSwitch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAnimatePress || !ButtonMesh)
	{
		return;
	}

	// Target is one of the two stable positions computed once in BeginPlay.
	// Reading and writing relative location keeps everything in local space
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

	if (!bIsEnabled)
	{
		return;
	}

	// bHasTriggered is the authoritative one-shot spent guard.  bIsEnabled is
	// also false after a one-shot fires (SetActivated(false) is called below),
	// so in practice this check is only reached when a race condition re-enables
	// the switch during the same frame.  It is cheap and makes intent explicit.
	if (bOneShot && bHasTriggered)
	{
		return;
	}

	SendTargetCommands();
	ActivatePressTargets();

	if (bOneShot)
	{
		bHasTriggered = true;
		// Self-disable: sinks the button and blocks future overlaps.
		// An external SetActivated(true) clears bHasTriggered and re-arms it.
		SetActivated(false);
	}
	else
	{
		SetPressedState(true);
	}
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

	// One-shot switches self-disabled on press; bIsEnabled is already false.
	// The guard below covers both the one-shot and the externally-disabled cases.
	if (!bIsEnabled)
	{
		return;
	}

	// Reusable: return the button to the elevated / ready visual.
	// Target commands are NOT reversed here — the platform (or other target)
	// keeps whatever state it was sent to when the switch was pressed.
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

void AFloorSwitch::SendTargetCommands() const
{
	for (const FSwitchTarget& Entry : LinkedTargets)
	{
		if (!Entry.Target)
		{
			continue;
		}

		if (ISwitchable* Switchable = Cast<ISwitchable>(Entry.Target.Get()))
		{
			Switchable->SetActivated(Entry.bActivate);
		}
	}
}

void AFloorSwitch::SetActivated(bool bActivate)
{
	bIsEnabled = bActivate;

	if (bActivate)
	{
		// Re-arm: clear the one-shot spent flag so the switch can fire again.
		// Harmless for reusable switches (bHasTriggered is always false there).
		bHasTriggered = false;
	}

	// Pressed = Disabled, Released = Enabled — the visual state always matches
	// the enabled state so the player can read the switch at a glance.
	//
	//   SetActivated(true)  — raises the button (IsPressed = 0); ready to use.
	//   SetActivated(false) — sinks  the button (IsPressed = 1); spent / blocked.
	SetPressedState(!bIsEnabled);
}

void AFloorSwitch::ActivatePressTargets() const
{
	for (AActor* Target : TargetsToEnableOnPress)
	{
		if (ISwitchable* Switchable = Cast<ISwitchable>(Target))
		{
			Switchable->SetActivated(true);
		}
	}

	for (AActor* Target : TargetsToDisableOnPress)
	{
		if (ISwitchable* Switchable = Cast<ISwitchable>(Target))
		{
			Switchable->SetActivated(false);
		}
	}
}

void AFloorSwitch::InitMaterialInstances()
{
	// For each mesh: if slot 0 is already a DMI, reuse it — this prevents
	// DMI-of-DMI chaining on repeated OnConstruction calls.  If it is a plain
	// UMaterialInterface, create a fresh DMI and assign it back to the slot.
	// Either way, the cached pointer is left valid (or null if no material).

	if (BaseMesh)
	{
		UMaterialInterface* Mat = BaseMesh->GetMaterial(0);
		if (UMaterialInstanceDynamic* ExistingDMI = Cast<UMaterialInstanceDynamic>(Mat))
		{
			// Already a DMI — reuse it; no new object is created.
			BaseMaterialInstance = ExistingDMI;
		}
		else if (Mat)
		{
			BaseMaterialInstance = UMaterialInstanceDynamic::Create(Mat, this);
			BaseMesh->SetMaterial(0, BaseMaterialInstance);
		}
	}

	if (ButtonMesh)
	{
		UMaterialInterface* Mat = ButtonMesh->GetMaterial(0);
		if (UMaterialInstanceDynamic* ExistingDMI = Cast<UMaterialInstanceDynamic>(Mat))
		{
			ButtonMaterialInstance = ExistingDMI;
		}
		else if (Mat)
		{
			ButtonMaterialInstance = UMaterialInstanceDynamic::Create(Mat, this);
			ButtonMesh->SetMaterial(0, ButtonMaterialInstance);
		}
	}
}

void AFloorSwitch::ApplyMaterialParameters() const
{
	// SetVectorParameterValue is a no-op when the parameter does not exist in
	// the material, so both calls are safe regardless of material setup.

	if (BaseMaterialInstance)
	{
		BaseMaterialInstance->SetVectorParameterValue(TEXT("TileOffset"), TileOffset_Base);
	}

	if (ButtonMaterialInstance)
	{
		ButtonMaterialInstance->SetVectorParameterValue(TEXT("TileOffset"), TileOffset_Button);
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
	// Animated case: Tick() drives the interpolation each frame toward
	// PressedLocation or ReleasedLocation — no offset is applied here.
}

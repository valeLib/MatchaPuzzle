// Copyright Epic Games, Inc. All Rights Reserved.

#include "FloorSwitch.h"
#include "Switchable.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

AFloorSwitch::AFloorSwitch()
{
	// No Tick needed — all logic is driven by overlap events
	PrimaryActorTick.bCanEverTick = false;

	// Trigger volume as root so the actor's transform drives the switch position
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	RootComponent = TriggerVolume;

	TriggerVolume->SetBoxExtent(FVector(50.f, 50.f, 25.f));
	TriggerVolume->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	TriggerVolume->SetGenerateOverlapEvents(true);

	// Visible mesh — attach to root so it moves with the actor.
	// Assign a static mesh in the Details panel or a child Blueprint.
	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));
	SwitchMesh->SetupAttachment(RootComponent);
	SwitchMesh->SetCollisionProfileName(TEXT("NoCollision"));
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
		return;
	}

	// One-shot: ignore subsequent overlaps after the first activation
	if (bOneShot && bHasTriggered)
	{
		return;
	}

	bHasTriggered = true;
	SetTargetsActivated(true);
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

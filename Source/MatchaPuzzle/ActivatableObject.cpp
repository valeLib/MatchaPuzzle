// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActivatableObject.h"
#include "Components/StaticMeshComponent.h"

AActivatableObject::AActivatableObject()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
}

void AActivatableObject::BeginPlay()
{
	Super::BeginPlay();

	// Apply the initial visibility/collision state chosen in the Details panel.
	if (bStartHidden)
	{
		Deactivate_Implementation();
	}
	else
	{
		Activate_Implementation();
	}
}

void AActivatableObject::Activate_Implementation()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

void AActivatableObject::Deactivate_Implementation()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

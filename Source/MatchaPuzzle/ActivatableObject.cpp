// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActivatableObject.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"

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
	SetActivationState(true);
}

void AActivatableObject::Deactivate_Implementation()
{
	SetActivationState(false);
}

void AActivatableObject::SetActivated(bool bActivated)
{
	SetActivationState(bActivated);
}

void AActivatableObject::SetActivationState(bool bActive)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);

	if (bAffectChildComponents)
	{
		const ECollisionEnabled::Type CollisionMode =
			bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;

		TArray<UActorComponent*> Components;
		GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
			{
				SceneComp->SetHiddenInGame(!bActive, false);
			}
			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
			{
				PrimComp->SetCollisionEnabled(CollisionMode);
			}
		}
	}

	if (bAffectAttachedActors)
	{
		TArray<AActor*> AttachedActors;
		GetAttachedActors(AttachedActors, /*bResetArray=*/true, bRecursiveAttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			if (Attached)
			{
				Attached->SetActorHiddenInGame(!bActive);
				Attached->SetActorEnableCollision(bActive);
			}
		}
	}
}

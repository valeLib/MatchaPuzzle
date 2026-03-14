// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollectiblePickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"

ACollectiblePickup::ACollectiblePickup()
{
	PrimaryActorTick.bCanEverTick = true;

	// Sphere trigger as root — detects the player, drives the actor's position.
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent   = CollisionSphere;

	CollisionSphere->SetSphereRadius(CollisionRadius);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	CollisionSphere->SetGenerateOverlapEvents(true);

	// Visual mesh — no collision so the player walks through it freely.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
}

void ACollectiblePickup::BeginPlay()
{
	Super::BeginPlay();

	CollisionSphere->OnComponentBeginOverlap.AddDynamic(
		this, &ACollectiblePickup::OnOverlapBegin);
}

void ACollectiblePickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsCollected)
	{
		TickCollectAnimation(DeltaTime);
	}
	else if (!FMath::IsNearlyZero(RotationSpeed))
	{
		// Idle spin — Yaw is local Z, which is the natural "spin in place" axis
		// for a top-down collectible.
		AddActorLocalRotation(FRotator(0.f, RotationSpeed * DeltaTime, 0.f));
	}
}

void ACollectiblePickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Sync the Static Mesh asset so swapping PickupMesh in the Details panel
	// updates the viewport immediately without entering PIE.
	if (PickupMesh)
	{
		MeshComponent->SetStaticMesh(PickupMesh);
	}

	// Keep the trigger sphere radius in sync with the editor-exposed property.
	CollisionSphere->SetSphereRadius(CollisionRadius);
}

// ── BlueprintNativeEvent ────────────────────────────────────────────────────────

void ACollectiblePickup::OnCollected_Implementation()
{
	// Default: intentionally empty.
	// Override in Blueprint to play a pickup sound, spawn a Niagara burst,
	// notify the game mode, add score, etc.
}

// ── Overlap callback ────────────────────────────────────────────────────────────

void ACollectiblePickup::OnOverlapBegin(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor*              OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32                /*OtherBodyIndex*/,
	bool                 /*bFromSweep*/,
	const FHitResult&    /*SweepResult*/)
{
	// Gate first — ensures HandleCollection runs at most once even if multiple
	// overlap events fire on the same frame.
	if (bIsCollected)
	{
		return;
	}

	if (!IsLocalPlayer(OtherActor))
	{
		return;
	}

	HandleCollection();
}

// ── Helpers ─────────────────────────────────────────────────────────────────────

bool ACollectiblePickup::IsLocalPlayer(const AActor* OtherActor)
{
	if (!OtherActor)
	{
		return false;
	}
	const APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	return OverlappingPawn && OverlappingPawn->IsPlayerControlled();
}

void ACollectiblePickup::HandleCollection()
{
	// Set gate before anything else — prevents any re-entrant overlap callbacks.
	bIsCollected = true;

	// Disable collision immediately so no other actor or frame can re-trigger.
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Record the starting state for the animation.  All motion is computed
	// relative to these snapshots so there is no incremental drift.
	CollectStartLocation = GetActorLocation();
	CollectStartScale    = MeshComponent->GetRelativeScale3D();

	// Fire the Blueprint hook.  Blueprint children play sounds, particles, etc.
	OnCollected();
}

void ACollectiblePickup::TickCollectAnimation(float DeltaTime)
{
	// Early-out once the animation has already been completed and FinishAnimation
	// has been called.  Subsequent ticks are no-ops until Tick is fully disabled.
	if (CollectAnimTimer >= CollectAnimDuration)
	{
		return;
	}

	// Clamp to duration so the final frame lands exactly at progress = 1.
	CollectAnimTimer = FMath::Min(CollectAnimTimer + DeltaTime, CollectAnimDuration);

	// Normalised progress [0, 1].
	const float Progress         = CollectAnimTimer / CollectAnimDuration;
	const float SmoothedProgress = FMath::SmoothStep(0.f, 1.f, Progress);

	// ── Rise ──
	// Absolute positioning: always computed from the recorded start location,
	// never accumulated.  Frame-rate-independent and drift-free.
	SetActorLocation(CollectStartLocation + FVector(0.f, 0.f, RiseHeight * SmoothedProgress));

	// ── Scale pop ──
	// Scales uniformly from the mesh's initial relative scale up to
	// CollectScaleMultiplier times that scale.
	const float   ScaleFactor = FMath::Lerp(1.f, CollectScaleMultiplier, SmoothedProgress);
	MeshComponent->SetRelativeScale3D(CollectStartScale * ScaleFactor);

	// Trigger completion exactly once, on the tick that reaches the end.
	if (CollectAnimTimer >= CollectAnimDuration)
	{
		FinishAnimation();
	}
}

void ACollectiblePickup::FinishAnimation()
{
	// Hide the mesh — the actor position / scale have reached their final values.
	MeshComponent->SetVisibility(false);

	// Stop ticking entirely.  The actor is either about to be destroyed or will
	// remain hidden; either way there is nothing left to do each frame.
	SetActorTickEnabled(false);

	// Schedule the final destroy / hide step.  A non-zero DestroyDelay keeps
	// the actor alive long enough for any Blueprint-spawned sounds or particles
	// (triggered in OnCollected) to finish before the actor is garbage-collected.
	if (DestroyDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			DestroyTimerHandle,
			this, &ACollectiblePickup::ExecutePostCollect,
			DestroyDelay,
			/*bLoop=*/ false);
	}
	else
	{
		ExecutePostCollect();
	}
}

void ACollectiblePickup::ExecutePostCollect()
{
	if (bDestroyOnCollect)
	{
		Destroy();
	}
	// If !bDestroyOnCollect the actor remains in the level — invisible, with
	// collision disabled and tick stopped.  Blueprint logic can respawn it,
	// re-enable it, or use it as a data marker.
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CollectiblePickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UStaticMesh;

/**
 *  A reusable collectible pickup actor designed to be placed in the level or
 *  sub-classed in Blueprint.
 *
 *  Component layout
 *  ────────────────
 *   CollisionSphere (USphereComponent, root — OverlapOnlyPawn)
 *   └─ MeshComponent (UStaticMeshComponent — NoCollision, visual only)
 *
 *  Idle behaviour
 *  ──────────────
 *  The actor continuously rotates around its local Z axis at RotationSpeed
 *  degrees per second.  Set RotationSpeed = 0 to disable.
 *
 *  Collection sequence
 *  ───────────────────
 *  1. Player pawn overlaps CollisionSphere.
 *  2. bIsCollected is set to true (gate — prevents double-collection).
 *  3. Collision is disabled.
 *  4. OnCollected() fires — override in Blueprint to play sounds / particles /
 *     update the score without touching C++.
 *  5. Each tick for CollectAnimDuration seconds the actor rises by RiseHeight
 *     and the mesh scales up to CollectScaleMultiplier.  Uses absolute
 *     positioning (no incremental integration) so the motion is never jittery.
 *  6. When the animation finishes the mesh is hidden and Tick is disabled.
 *  7. After DestroyDelay seconds the actor is destroyed (or left hidden when
 *     bDestroyOnCollect = false, useful for respawnable pickups in Blueprint).
 *
 *  Setup
 *  ─────
 *   1. Place ACollectiblePickup or a Blueprint child in the level.
 *   2. Assign a Static Mesh via the PickupMesh property — the viewport updates
 *      immediately without entering PIE.
 *   3. Tune appearance and animation properties in the Details panel.
 */
UCLASS()
class ACollectiblePickup : public AActor
{
	GENERATED_BODY()

	/** Overlap trigger — detects the player pawn. Acts as the actor root. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	USphereComponent* CollisionSphere;

	/** Visual mesh for the collectible.  Assign the mesh via PickupMesh below. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MeshComponent;

public:

	ACollectiblePickup();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Syncs PickupMesh and CollisionRadius to the components in the editor
	 * so viewport feedback is immediate when either property changes.
	 */
	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * Called the moment the player overlaps this pickup, before the animation
	 * begins.  Override in Blueprint to play a sound, spawn a particle, add
	 * score, etc.  The default C++ implementation is intentionally empty.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Collectible")
	void OnCollected();

protected:

	virtual void OnCollected_Implementation();

	// ── Appearance ────────────────────────────────────────────────────────────

	/**
	 * Static mesh displayed by this pickup.  Assign any mesh asset here
	 * (coin, diamond, heart…) — changing this property updates the viewport
	 * immediately without entering PIE.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible|Appearance")
	TObjectPtr<UStaticMesh> PickupMesh;

	/**
	 * Radius of the sphere trigger in world units.  Changing this property
	 * updates the sphere immediately in the editor viewport.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible|Appearance",
		meta = (ClampMin = "5"))
	float CollisionRadius = 50.f;

	// ── Idle animation ────────────────────────────────────────────────────────

	/**
	 * Continuous rotation speed around the actor's local Z axis in degrees per
	 * second.  Set to 0 to disable idle rotation entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible|IdleAnimation")
	float RotationSpeed = 90.f;

	// ── Collect animation ─────────────────────────────────────────────────────

	/**
	 * How far upward (units) the pickup floats during the collection animation.
	 * The motion uses absolute positioning so it is smooth regardless of
	 * frame rate.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible|CollectAnimation",
		meta = (ClampMin = "0"))
	float RiseHeight = 50.f;

	/**
	 * Scale multiplier applied to the mesh at the end of the collection
	 * animation.  1.0 = no change.  Values above 1 make the pickup "pop"
	 * outward before disappearing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible|CollectAnimation",
		meta = (ClampMin = "0.1"))
	float CollectScaleMultiplier = 1.5f;

	/**
	 * Total duration of the collect animation in seconds.  The rise and scale
	 * both complete within this window using a SmoothStep ease curve.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible|CollectAnimation",
		meta = (ClampMin = "0.01"))
	float CollectAnimDuration = 0.4f;

	// ── Post-collect ──────────────────────────────────────────────────────────

	/**
	 * When true (default), the actor is destroyed after the animation and
	 * DestroyDelay.  Set to false for pickups that should stay in the level
	 * hidden (e.g. to be respawned later from Blueprint logic).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible|PostCollect")
	bool bDestroyOnCollect = true;

	/**
	 * Extra seconds to wait after the animation finishes before destroying the
	 * actor.  Use this to keep the actor alive long enough for a sound or
	 * particle spawned in OnCollected() to finish playing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible|PostCollect",
		meta = (ClampMin = "0"))
	float DestroyDelay = 0.2f;

private:

	// ── State ─────────────────────────────────────────────────────────────────

	/** Set to true the moment a player overlaps. Prevents double-collection. */
	bool bIsCollected = false;

	/** Seconds elapsed since the collection animation started. */
	float CollectAnimTimer = 0.f;

	/** World location of the actor the frame the collection animation began. */
	FVector CollectStartLocation = FVector::ZeroVector;

	/** Relative scale of MeshComponent the frame the animation began. */
	FVector CollectStartScale = FVector::OneVector;

	/** Used to schedule the post-animation destroy / hide. */
	FTimerHandle DestroyTimerHandle;

	// ── Overlap callback ──────────────────────────────────────────────────────

	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor*              OtherActor,
		UPrimitiveComponent* OtherComp,
		int32                OtherBodyIndex,
		bool                 bFromSweep,
		const FHitResult&    SweepResult);

	// ── Helpers ───────────────────────────────────────────────────────────────

	/** Returns true if OtherActor is a locally controlled player pawn. */
	static bool IsLocalPlayer(const AActor* OtherActor);

	/**
	 * Begins the collection sequence: sets the gate, disables collision,
	 * records animation start state, fires OnCollected().
	 */
	void HandleCollection();

	/**
	 * Advances the collection animation each tick.
	 * Uses absolute positioning — no integration drift possible.
	 * Calls FinishAnimation() once CollectAnimDuration is reached.
	 */
	void TickCollectAnimation(float DeltaTime);

	/**
	 * Called once when the animation completes.
	 * Hides the mesh, disables tick, then schedules ExecutePostCollect()
	 * after DestroyDelay seconds.
	 */
	void FinishAnimation();

	/**
	 * Scheduled by FinishAnimation().  Destroys the actor if bDestroyOnCollect
	 * is true; otherwise leaves it hidden in the level.
	 */
	void ExecutePostCollect();
};

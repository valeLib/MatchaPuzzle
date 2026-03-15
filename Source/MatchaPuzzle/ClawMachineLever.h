// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClawMachineLever.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 *  A claw-machine-style directional lever.
 *
 *  Unlike ALever (which fires a binary on/off when the player enters its
 *  trigger), AClawMachineLever is actively steered by the player while they
 *  remain inside the interaction sphere.  Horizontal and vertical control
 *  values are sent to the linked target each frame the values change, via the
 *  IClawControllable interface.
 *
 *  Component layout:
 *   Root (InteractionTrigger — USphereComponent)
 *   ├─ BaseMesh   — fixed base, never moves
 *   └─ LeverMesh  — rotates on local axes to reflect control values
 *
 *  Controls (active only while the player is inside InteractionTrigger):
 *   E — increase HorizontalValue  (lever leans right)
 *   R — decrease HorizontalValue  (lever leans left)
 *   A — increase VerticalValue    (lever pushes forward / down)
 *
 *  Visual rotation:
 *   Horizontal control → Roll  offset from the initial rest rotation
 *   Vertical control   → Pitch offset from the initial rest rotation
 *   Both offsets are scaled to LeverHorizontalAngleRange / LeverDownAngleRange.
 *   The rest rotation is captured once at BeginPlay so the lever never drifts.
 *
 *  Hover feedback (choose one or both in the Details panel):
 *   • bUseHoverMaterial   — writes IsHovered (0/1) to LeverMesh DMI slot 0
 *   • bUseCustomDepth     — enables CustomDepth + stencil on LeverMesh
 */
UCLASS()
class AClawMachineLever : public AActor
{
	GENERATED_BODY()

	/** Sphere that detects player proximity and enables lever interaction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	USphereComponent* InteractionTrigger;

	/** Fixed base geometry — never moves. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BaseMesh;

	/** Lever geometry — rotates to reflect current control values. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* LeverMesh;

public:

	AClawMachineLever();

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ── Controlled target ─────────────────────────────────────────────────────

	/**
	 * Actor to receive directional input from this lever.
	 * Must implement IClawControllable; non-implementing actors are silently
	 * ignored.  Set this to a platform or any other actor that responds to
	 * SetLeverInput(float Horizontal, float Vertical).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Target")
	TObjectPtr<AActor> ControlledTarget;

	// ── Control values ────────────────────────────────────────────────────────

	/**
	 * Current horizontal position of the lever.
	 * Modified by E / R input while the player is inside the trigger.
	 * Clamped to [HorizontalMin, HorizontalMax] at all times.
	 * Negative = left, positive = right.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float HorizontalValue = 0.f;

	/** Left boundary of the horizontal range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float HorizontalMin = -1.f;

	/** Right boundary of the horizontal range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float HorizontalMax = 1.f;

	/**
	 * Current vertical (down-press) position of the lever.
	 * Modified by A input while the player is inside the trigger.
	 * Clamped to [VerticalMin, VerticalMax] at all times.
	 * VerticalMin = fully up (rest), VerticalMax = fully pressed down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float VerticalValue = 0.f;

	/** Upper boundary of the vertical range (rest / up position). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float VerticalMin = 0.f;

	/** Lower boundary of the vertical range (fully pressed down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float VerticalMax = 1.f;

	/**
	 * Rate at which horizontal and vertical values change per second while
	 * the corresponding key is held.
	 * Example: InputStep = 1.0 with HorizontalMin/Max = [-1, 1] means the lever
	 * traverses the full range in two seconds of continuous input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control",
		meta = (ClampMin = "0.01"))
	float InputStep = 1.f;

	// ── Visual motion ─────────────────────────────────────────────────────────

	/**
	 * Maximum degrees LeverMesh leans left or right from its rest rotation.
	 * Mapped from the full [HorizontalMin, HorizontalMax] range:
	 *   HorizontalMin → -LeverHorizontalAngleRange (left)
	 *   0 (midpoint)  → 0° (upright, if the range is symmetric)
	 *   HorizontalMax → +LeverHorizontalAngleRange (right)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Visual",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float LeverHorizontalAngleRange = 30.f;

	/**
	 * Maximum degrees LeverMesh leans forward from its rest rotation when the
	 * lever is pushed fully down.
	 * Mapped from [VerticalMin, VerticalMax]:
	 *   VerticalMin → 0° (rest)
	 *   VerticalMax → +LeverDownAngleRange (fully forward)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Visual",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float LeverDownAngleRange = 20.f;

	/** Rotation interpolation speed in Tick when bSmoothMotion is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Visual",
		meta = (ClampMin = "0.1"))
	float LeverMoveSpeed = 8.f;

	/** When true LeverMesh interpolates smoothly to its target; when false it snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Visual")
	bool bSmoothMotion = true;

	// ── Hover feedback ────────────────────────────────────────────────────────

	/**
	 * When true, a Dynamic Material Instance is created for LeverMesh slot 0
	 * and its "IsHovered" scalar parameter is set to 1.0 on entry and 0.0 on exit.
	 * Fails gracefully if LeverMesh has no material assigned.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Hover")
	bool bUseHoverMaterial = true;

	/**
	 * When true, enables CustomDepth rendering on LeverMesh while the player
	 * is inside the trigger.  Use this with a post-process outline effect.
	 * Requires r.CustomDepth = 3 in DefaultEngine.ini.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Hover")
	bool bUseCustomDepth = false;

	/** Stencil value written to LeverMesh when CustomDepth is active (0–255). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Hover",
		meta = (EditCondition = "bUseCustomDepth", ClampMin = "0", ClampMax = "255"))
	int32 CustomDepthStencilValue = 1;

private:

	/** True while a local player pawn is inside InteractionTrigger. */
	bool bPlayerInRange = false;

	/** Weak reference to the controller currently interacting; cleared on exit. */
	TWeakObjectPtr<APlayerController> InteractingController;

	/** LeverMesh relative rotation captured once at BeginPlay — the neutral origin. */
	FRotator InitialLeverRotation = FRotator::ZeroRotator;

	/**
	 * Dynamic material instance created from LeverMesh material slot 0.
	 * Null if bUseHoverMaterial is false or LeverMesh has no material.
	 */
	UPROPERTY()
	UMaterialInstanceDynamic* LeverMaterialInstance = nullptr;

	// ── Overlap callbacks ─────────────────────────────────────────────────────

	UFUNCTION()
	void OnTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor*              OtherActor,
		UPrimitiveComponent* OtherComp,
		int32                OtherBodyIndex,
		bool                 bFromSweep,
		const FHitResult&    SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor*              OtherActor,
		UPrimitiveComponent* OtherComp,
		int32                OtherBodyIndex);

	// ── Helpers ───────────────────────────────────────────────────────────────

	/** Returns true if OtherActor is a pawn controlled by a local player. */
	static bool IsLocalPlayer(const AActor* OtherActor);

	/** Extracts the APlayerController from a local player pawn. */
	static APlayerController* GetPlayerController(const AActor* PlayerActor);

	/**
	 * Applies or removes hover visual feedback on LeverMesh.
	 * Updates IsHovered material parameter and/or CustomDepth based on
	 * bUseHoverMaterial / bUseCustomDepth.  Never touches BaseMesh.
	 */
	void SetHoverState(bool bHovered);

	/**
	 * Maps current HorizontalValue and VerticalValue to a target FRotator
	 * in local space, offset from InitialLeverRotation.
	 * Pure computation — reads state, does not write anything.
	 */
	FRotator ComputeTargetRotation() const;

	/**
	 * Sends current HorizontalValue and VerticalValue to ControlledTarget via
	 * IClawControllable::Execute_SetLeverInput.
	 * No-op if ControlledTarget is null or does not implement IClawControllable.
	 */
	void SendInputToTarget() const;
};

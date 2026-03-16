// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClawMachineLever.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class APuzzlePlayerController;

/**
 *  A claw-machine-style directional lever.
 *
 *  Interaction model:
 *   1. Player walks near the lever — InteractionTrigger overlap fires, the lever
 *      registers itself with APuzzlePlayerController as CurrentNearbyLever and
 *      hover visual feedback activates.
 *   2. Player presses E — APuzzlePlayerController calls BeginControl(PC).
 *      The lever enters controlled mode and the controller routes WASD to it.
 *   3. Player uses WASD — controller calls AddHorizontalInput / AddVerticalInput
 *      each frame.  The lever clamps the values and notifies its target.
 *   4. Player presses E again (or walks away) — controller calls EndControl().
 *      Normal movement resumes.
 *
 *  The lever is responsible for:
 *   - overlap / proximity detection
 *   - hover and controlled visual feedback
 *   - storing and clamping HorizontalValue / VerticalValue
 *   - computing a stable LeverMesh target rotation from InitialLeverRotation
 *   - smooth interpolation of LeverMesh in Tick
 *   - notifying ControlledTarget via IClawControllable
 *
 *  The controller is responsible for:
 *   - deciding when to begin / end lever control
 *   - routing WASD input to the active lever
 *
 *  Vertical convention:
 *   VerticalMax = 0   (neutral resting position — lever cannot go higher)
 *   VerticalMin = -1  (fully pushed down)
 *   W key → increase toward VerticalMax (0), clamped so lever never rises above neutral
 *   S key → decrease toward VerticalMin
 *
 *  Component layout:
 *   Root (InteractionTrigger — USphereComponent)
 *   ├─ BaseMesh   — fixed base, never moves
 *   └─ LeverMesh  — rotates on local axes to reflect control values
 *
 *  Visual rotation (from InitialLeverRotation captured once at BeginPlay):
 *   Roll  offset → horizontal lean (left / right)
 *   Pitch offset → forward / down lean
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

	/**
	 * World-space widget shown above the lever when the player is nearby and
	 * not yet controlling it.  Assign a widget class (e.g. WBP_InteractPrompt)
	 * in the Details panel; the component is hidden by default and shown /
	 * hidden automatically by the overlap and control logic.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* InteractPromptWidget;

public:

	AClawMachineLever();

	// ── Controller-facing API ─────────────────────────────────────────────────

	/**
	 * Called by APuzzlePlayerController when the player presses E near this lever.
	 * Marks this lever as actively controlled, stores the controller reference for
	 * cleanup callbacks, and activates the controlled visual state.
	 * Safe to call if already controlled — updates the controller reference.
	 */
	void BeginControl(APuzzlePlayerController* Controller);

	/**
	 * Called by APuzzlePlayerController (or internally on proximity exit) to
	 * release control.  Notifies the controller via OnLeverControlEnded so it can
	 * clear its ActiveControlledLever reference.  Deactivates controlled visuals.
	 */
	void EndControl();

	/**
	 * Adds a delta to HorizontalValue this frame.
	 * Value is the raw Enhanced Input axis (−1..1, positive = D/right).
	 * DeltaTime is used for rate scaling so movement is frame-rate independent.
	 * Clamps result to [HorizontalMin, HorizontalMax] and notifies the target.
	 * No-op if the lever is not in controlled mode.
	 */
	void AddHorizontalInput(float Value, float DeltaTime);

	/**
	 * Adds a delta to VerticalValue this frame.
	 * Value is the raw Enhanced Input axis (−1..1, positive = W/up, negative = S/down).
	 * Clamped so the lever can never rise above VerticalMax (neutral = 0).
	 * No-op if the lever is not in controlled mode.
	 */
	void AddVerticalInput(float Value, float DeltaTime);

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ── Interaction prompt ────────────────────────────────────────────────────

	/**
	 * Widget class displayed by InteractPromptWidget.
	 * Assign WBP_InteractPrompt (or any UUserWidget subclass) in the Details
	 * panel.  The widget is placed slightly above the lever in screen space.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Prompt")
	TSubclassOf<UUserWidget> InteractPromptWidgetClass;

	/**
	 * Offset from the actor root at which the prompt is displayed,
	 * in local space (Z = up).  Adjust if the lever mesh is taller or shorter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Prompt")
	FVector PromptOffset = FVector(0.f, 0.f, 120.f);

	// ── Controlled target ─────────────────────────────────────────────────────

	/**
	 * Actor to receive directional input from this lever.
	 * Must implement IClawControllable; non-implementing actors are silently
	 * ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Target")
	TObjectPtr<AActor> ControlledTarget;

	// ── Control values ────────────────────────────────────────────────────────

	/**
	 * Current horizontal position of the lever.
	 * Negative = left (A key), positive = right (D key).
	 * Clamped to [HorizontalMin, HorizontalMax].
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
	 * Current vertical position of the lever.
	 * 0 = neutral / resting (top).  Negative = pushed down.
	 * W key moves toward 0 (clamped at VerticalMax).
	 * S key moves toward VerticalMin.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float VerticalValue = 0.f;

	/**
	 * Maximum downward displacement (should be negative, e.g. −1).
	 * Lever cannot go below this value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float VerticalMin = -1.f;

	/**
	 * Neutral / resting position (should be 0 or the initial upright value).
	 * Lever cannot rise above this value — W key is clamped here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control")
	float VerticalMax = 0.f;

	/**
	 * Rate at which values change per second while a key is held.
	 * Applied as: Value += InputAxis * InputStep * DeltaTime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Control",
		meta = (ClampMin = "0.01"))
	float InputStep = 1.f;

	// ── Visual motion ─────────────────────────────────────────────────────────

	/**
	 * Maximum degrees LeverMesh leans left or right from its rest rotation.
	 * Horizontal centre maps to 0°; HorizontalMin/Max map to ±this value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Visual",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float LeverHorizontalAngleRange = 30.f;

	/**
	 * Maximum degrees LeverMesh leans forward when fully pushed down.
	 * VerticalMax (0 / neutral) → 0°.  VerticalMin → this value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Visual",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float LeverDownAngleRange = 20.f;

	/** Rotation interpolation speed when bSmoothMotion is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Visual",
		meta = (ClampMin = "0.1"))
	float LeverMoveSpeed = 8.f;

	/** Smooth rotation interpolation in Tick; false = snap instantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Visual")
	bool bSmoothMotion = true;

	// ── Hover / controlled feedback ───────────────────────────────────────────

	/**
	 * Creates a DMI from LeverMesh slot 0 and drives scalar parameters:
	 *   IsHovered    — 1.0 while the player is inside the trigger
	 *   IsControlled — 1.0 while the player is actively steering the lever
	 * Gracefully no-ops if the material or parameters are missing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Claw|Hover")
	bool bUseHoverMaterial = true;

	/**
	 * Enables CustomDepth rendering on LeverMesh while the player is in the
	 * trigger.  Use with a post-process outline effect.
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

	/** True while BeginControl has been called and EndControl has not. */
	bool bIsControlled = false;

	/**
	 * The controller that called BeginControl.
	 * Used to notify APuzzlePlayerController::OnLeverControlEnded when this
	 * lever releases control internally (e.g. player exits the trigger).
	 * Cleared in EndControl.
	 */
	TWeakObjectPtr<APuzzlePlayerController> ActiveController;

	/** LeverMesh relative rotation captured once at BeginPlay — the neutral origin. */
	FRotator InitialLeverRotation = FRotator::ZeroRotator;

	/**
	 * DMI created from LeverMesh material slot 0.
	 * Null if bUseHoverMaterial is false or no material is assigned.
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

	/**
	 * Applies or removes hover visual feedback (IsHovered material parameter
	 * and optional CustomDepth).  Never touches BaseMesh.
	 */
	void SetHoverState(bool bHovered);

	/**
	 * Applies or removes the actively-controlled visual feedback
	 * (IsControlled material parameter).  Never touches BaseMesh.
	 */
	void SetControlledState(bool bControlled);

	/**
	 * Maps current HorizontalValue and VerticalValue to a target FRotator in
	 * local space, offset from InitialLeverRotation.
	 * Pure computation — reads state only.
	 */
	FRotator ComputeTargetRotation() const;

	/**
	 * Shows or hides the InteractPromptWidget.
	 * The prompt is only shown when the player is in range AND not controlling
	 * the lever; all callers delegate here so the rule is enforced in one place.
	 */
	void SetPromptVisible(bool bVisible);

	/**
	 * Sends current HorizontalValue and VerticalValue to ControlledTarget via
	 * IClawControllable::Execute_SetLeverInput.
	 * No-op if ControlledTarget is null or does not implement IClawControllable.
	 */
	void SendInputToTarget() const;
};

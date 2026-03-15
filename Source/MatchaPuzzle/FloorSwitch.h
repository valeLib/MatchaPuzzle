// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Switchable.h"
#include "FloorSwitch.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 *  A pressure-sensitive floor switch that activates one or more ISwitchable
 *  actors when the player steps on it.
 *
 *  Pressed  ↔  Disabled:  a sunken button cannot be used.
 *  Released ↔  Enabled:   an elevated button is ready to be stepped on.
 *
 *  Two modes (bOneShot):
 *
 *  Reusable (bOneShot = false):
 *   Behaves like a held button.  Stepping on it presses (disables) it and
 *   activates LinkedTargets; stepping off releases (re-enables) it and
 *   deactivates them.  TargetsToEnableOnPress / TargetsToDisableOnPress are
 *   evaluated on press only.  The switch can be used any number of times.
 *
 *  One-shot (bOneShot = true):
 *   The switch fires once, self-disables (stays sunken), and ignores further
 *   overlaps.  An external actor must call SetActivated(true) to re-arm it
 *   (raises the button, makes it usable again).  Useful for permanent triggers
 *   (a bridge that stays open) and alternating puzzles (A enables B, B re-arms A).
 *
 *  SetActivated(true)  — raises the button (IsPressed = 0), marks enabled.
 *  SetActivated(false) — sinks  the button (IsPressed = 1), marks disabled.
 *
 *  bStartEnabled = false → button starts sunken/disabled.
 *
 *  Visual layout:
 *   Root (TriggerVolume)
 *   ├─ BaseMesh   — fixed, never moves
 *   └─ ButtonMesh — moves along local Z to show pressed / released state
 */
UCLASS()
class AFloorSwitch : public AActor, public ISwitchable
{
	GENERATED_BODY()

	/**
	 * Trigger volume that detects when a pawn enters and exits the switch area.
	 * Sized as a standard pressure plate by default — adjust extent in the editor.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerVolume;

	/** Fixed base geometry — never moves. Assign a mesh in the Details panel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BaseMesh;

	/** Button geometry — moves along local Z to indicate pressed / released state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* ButtonMesh;

public:

	AFloorSwitch();

	/**
	 * Called whenever a property is changed in the editor (and once before
	 * BeginPlay at runtime).  Responsible for creating dynamic material
	 * instances and pushing the TileOffset values so the viewport reflects
	 * changes immediately without entering PIE.
	 */
	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ── Linked targets ────────────────────────────────────────────────────────

	/**
	 * Actors driven by the press / release cycle.
	 * Each entry must implement ISwitchable; non-implementing actors are skipped.
	 *
	 * On press   → SetActivated(true)
	 * On release → SetActivated(false)  [reusable only]
	 *
	 * For press-only switch chaining use TargetsToEnableOnPress /
	 * TargetsToDisableOnPress alongside or instead of this array.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Targets")
	TArray<TObjectPtr<AActor>> LinkedTargets;

	/**
	 * ISwitchable actors that receive SetActivated(true) the moment this switch
	 * is successfully pressed.  Evaluated once per press; never on release.
	 * Use this to chain switches: pressing A enables B without any coupling in B.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Targets")
	TArray<TObjectPtr<AActor>> TargetsToEnableOnPress;

	/**
	 * ISwitchable actors that receive SetActivated(false) the moment this switch
	 * is successfully pressed.  Evaluated once per press; never on release.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Targets")
	TArray<TObjectPtr<AActor>> TargetsToDisableOnPress;

	// ── Behaviour ─────────────────────────────────────────────────────────────

	/**
	 * When true the switch fires once per arm cycle and then stops responding
	 * to overlaps.  Call SetActivated(true) to re-arm it.
	 *
	 * When false the switch behaves like a held button: press on enter,
	 * release on exit, reusable every time the player steps on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Behaviour")
	bool bOneShot = false;

	/**
	 * When false the switch starts disabled: it ignores all overlap events and
	 * shows the disabled material state.  Another switch can enable it at
	 * runtime by calling SetActivated(true) through the ISwitchable interface.
	 */
	UPROPERTY(EditAnywhere, Category = "Switch|Behaviour")
	bool bStartEnabled = true;

	// ── ISwitchable ───────────────────────────────────────────────────────────

	/**
	 * Enables or disables this switch at runtime.
	 *
	 * SetActivated(true)  — raises the button (IsPressed = 0), marks enabled.
	 *                       The switch is ready to be stepped on again.
	 *
	 * SetActivated(false) — sinks the button (IsPressed = 1), marks disabled.
	 *                       The switch ignores all overlap events until re-armed.
	 *
	 * The button is never auto-pressed by an external call; the player must
	 * step on the switch after it has been re-enabled.
	 */
	virtual void SetActivated(bool bActivate) override;

	// ── Button visual ─────────────────────────────────────────────────────────

	/** How far (cm) ButtonMesh lowers along its local Z when pressed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Button", meta = (ClampMin = "0.0"))
	float ButtonPressDepth = 8.f;

	/** Interpolation speed for the button animation (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Button", meta = (ClampMin = "0.1"))
	float ButtonMoveSpeed = 10.f;

	/** When true ButtonMesh interpolates smoothly; when false it snaps instantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Button")
	bool bAnimatePress = true;

	// ── Material parameters ───────────────────────────────────────────────────

	/**
	 * Value written to the TileOffset vector parameter on BaseMesh's material.
	 * XY = UV tile/offset; ZW available for additional shader use.
	 * Changing this in the Details panel updates the viewport immediately.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch|Visual|Material")
	FLinearColor TileOffset_Base = FLinearColor(0.f, 0.f, 0.f, 1.f);

	/**
	 * Value written to the TileOffset vector parameter on ButtonMesh's material.
	 * XY = UV tile/offset; ZW available for additional shader use.
	 * Changing this in the Details panel updates the viewport immediately.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch|Visual|Material")
	FLinearColor TileOffset_Button = FLinearColor(0.f, 0.f, 0.f, 1.f);

private:

	/** Runtime enabled state — initialised from bStartEnabled in BeginPlay.
	 *  false means the button is sunken/disabled (IsPressed = 1). */
	bool bIsEnabled = true;

	/** Current visual pressed state of the button */
	bool bIsPressed = false;

	/** ButtonMesh relative location when released (stored once at BeginPlay) */
	FVector ReleasedLocation = FVector::ZeroVector;

	/** ButtonMesh relative location when fully pressed (derived once at BeginPlay) */
	FVector PressedLocation = FVector::ZeroVector;

	/**
	 * Dynamic material instance for ButtonMesh slot 0.
	 * Drives IsPressed (0/1) and TileOffset vector parameters.
	 * Created or reused in InitMaterialInstances().
	 */
	UPROPERTY()
	UMaterialInstanceDynamic* ButtonMaterialInstance = nullptr;

	/**
	 * Dynamic material instance for BaseMesh slot 0.
	 * Drives TileOffset vector parameters.
	 * Created or reused in InitMaterialInstances().
	 */
	UPROPERTY()
	UMaterialInstanceDynamic* BaseMaterialInstance = nullptr;

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

	/** Returns true if OtherActor is a pawn controlled by a local player */
	static bool IsLocalPlayer(const AActor* OtherActor);

	/**
	 * Calls SetActivated(bActivate) on every ISwitchable entry in LinkedTargets.
	 * Non-implementing actors are skipped.
	 */
	void SetTargetsActivated(bool bActivate) const;

	/**
	 * Calls SetActivated(true)  on every entry in TargetsToEnableOnPress and
	 * SetActivated(false) on every entry in TargetsToDisableOnPress.
	 * Invoked once per successful press event; never on release.
	 */
	void ActivatePressTargets() const;

	/**
	 * Ensures BaseMaterialInstance and ButtonMaterialInstance are valid DMIs
	 * pointing at the current slot-0 material of each mesh.
	 *
	 * Safe to call repeatedly: if slot 0 is already a DMI (i.e. this function
	 * ran previously) it reuses the existing instance and skips creation,
	 * preventing DMI-of-DMI chaining across multiple OnConstruction calls.
	 * If the mesh has no material, the corresponding pointer remains null.
	 */
	void InitMaterialInstances();

	/**
	 * Writes TileOffset_Base to BaseMaterialInstance and TileOffset_Button to
	 * ButtonMaterialInstance.  No-op if either instance is null or if the
	 * TileOffset parameter does not exist in the material.
	 */
	void ApplyMaterialParameters() const;

	/**
	 * Updates the button visual state: sets bIsPressed, drives the IsPressed
	 * material parameter, and (when bAnimatePress is false) snaps ButtonMesh
	 * immediately.  When bAnimatePress is true, Tick drives the interpolation.
	 */
	void SetPressedState(bool bPressed);
};

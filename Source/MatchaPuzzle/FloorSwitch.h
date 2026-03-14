// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FloorSwitch.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 *  A pressure-sensitive floor switch that activates one or more ISwitchable
 *  actors when the player steps on it.
 *
 *  Setup:
 *   1. Place the actor in the level.
 *   2. Populate LinkedTargets with any actors that implement ISwitchable
 *      (AMovingPlatform in SwitchControlled mode, ARotatingPlatform, ALever…).
 *   3. Optionally enable bOneShot so targets stay activated permanently after
 *      the first trigger regardless of whether the player remains on the switch.
 *
 *  No Tick is used — all logic is event-driven via overlap callbacks.
 */
UCLASS()
class AFloorSwitch : public AActor
{
	GENERATED_BODY()

	/**
	 * Trigger volume that detects when a pawn enters and exits the switch area.
	 * Sized as a standard pressure plate by default — adjust extent in the editor.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerVolume;

	/** Visible geometry for the switch (pressure plate, button, etc.) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SwitchMesh;

public:

	AFloorSwitch();

protected:

	virtual void BeginPlay() override;

	// ── Linked targets ────────────────────────────────────────────────────────

	/**
	 * Actors to activate when the player steps on this switch.
	 * Each entry must implement ISwitchable; non-implementing actors are skipped.
	 * Accepts AMovingPlatform (SwitchControlled mode), ARotatingPlatform, ALever,
	 * or any other ISwitchable actor placed in the level.
	 *
	 * NOTE: if you had entries in the old LinkedPlatforms array they must be
	 * re-assigned here — the property was renamed as part of the ISwitchable
	 * refactor so that FloorSwitch is no longer coupled to a specific class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Targets")
	TArray<TObjectPtr<AActor>> LinkedTargets;

	// ── Behaviour ─────────────────────────────────────────────────────────────

	/**
	 * When true the switch fires once and never calls SetActivated(false),
	 * regardless of whether the player stays on or leaves the switch.
	 * Use this for doors or bridges that should stay open permanently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Behaviour")
	bool bOneShot = false;

	/**
	 * When true each entry toggles the platforms between activated and deactivated.
	 * Stepping on the switch a second time sends them back to their start position.
	 * Requires bReturnWhenReleased = true on the linked platforms to reverse correctly.
	 * Incompatible with bOneShot — bToggleable takes priority when both are set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch|Behaviour")
	bool bToggleable = false;

private:

	/** Prevents a one-shot switch from re-firing after its first activation */
	bool bHasTriggered = false;

	/** Tracks the current on/off state for toggleable switches */
	bool bToggleState = false;

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

	/** Calls SetActivated(bActivate) on every ISwitchable entry in LinkedTargets */
	void SetTargetsActivated(bool bActivate) const;
};

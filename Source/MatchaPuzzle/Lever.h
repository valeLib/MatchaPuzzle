// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActivatableTarget.h"
#include "Lever.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Axis around which the lever handle rotates during its on/off animation */
UENUM(BlueprintType)
enum class ELeverAxis : uint8
{
	Pitch   UMETA(DisplayName = "Pitch (forward / back)"),
	Roll    UMETA(DisplayName = "Roll (side to side)"),
	Yaw     UMETA(DisplayName = "Yaw (twist)")
};

// ── Per-target activatable entry ──────────────────────────────────────────────

/**
 *  One row in the ActivatableTargets array.
 *
 *  Each entry pairs an actor (that must implement IActivatableTarget) with its
 *  own independent timing settings so different targets on the same lever can
 *  activate at different delays and stay on for different durations.
 */
USTRUCT(BlueprintType)
struct FActivatableTargetEntry
{
	GENERATED_BODY()

	/**
	 * Actor that will receive Activate() and Deactivate() calls.
	 * Must implement IActivatableTarget (add the interface in Blueprint Class
	 * Settings, or inherit IActivatableTarget in C++).
	 * Entries that do not implement the interface are silently skipped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> Target = nullptr;

	/**
	 * Seconds to wait after the lever activates before this target turns ON.
	 * 0 = turn ON immediately when the lever is pulled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ClampMin = "0", Units = "s",
			DisplayName = "Activate Delay (s)"))
	float ActivateDelay = 0.f;

	/**
	 * How long (seconds) this target stays ON before being automatically turned OFF.
	 * 0 = stay ON indefinitely — the target will only turn OFF if the lever
	 * deactivates or is toggled off (unless bKeepActiveOnLeverOff is also set).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ClampMin = "0", Units = "s",
			DisplayName = "Active Duration (s)  [0 = indefinite]"))
	float ActiveDuration = 3.f;

	/**
	 * When true, the lever turning OFF (or being toggled off) will NOT call
	 * Deactivate() on this target — it stays ON until ActiveDuration expires
	 * naturally (or forever if ActiveDuration is also 0).
	 *
	 * Typical use: a one-way trigger — pull the lever once to reveal an object
	 * permanently, regardless of the lever's bToggleable or bOneShot setting.
	 *
	 * Re-activating the lever while the target is already ON is also a no-op
	 * for this entry (the running timer is left untouched).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (DisplayName = "Keep Active When Lever Off"))
	bool bKeepActiveOnLeverOff = false;
};

// ── Internal-only timer bookkeeping (not a USTRUCT, never exposed) ────────────

/**
 *  Holds the two timer handles and runtime ON/OFF state for one
 *  FActivatableTargetEntry.  Lives in a parallel private array, indexed
 *  identically to ActivatableTargets.  Not reflected or exposed anywhere.
 */
struct FActivatableTimerPair
{
	/** Fires after ActivateDelay — calls ALever::ActivateTarget(Index). */
	FTimerHandle ActivateTimer;

	/** Fires after ActiveDuration — calls ALever::DeactivateTarget(Index). */
	FTimerHandle DeactivateTimer;

	/** Whether this target is currently in the ON state. Used to decide
	 *  whether an immediate Deactivate() call is needed when the lever
	 *  deactivates or is toggled while the target is running. */
	bool bTargetIsActive = false;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 *  A lever actor that smoothly rotates its handle and activates linked
 *  targets when the player enters its trigger volume.
 *
 *  Two independent target systems operate in parallel:
 *
 *  LinkedTargets (ISwitchable)
 *  ───────────────────────────
 *  Direct binary control: SetActivated(true/false) is called immediately when
 *  the lever changes state.  Use this for platforms (AMovingPlatform,
 *  ARotatingPlatform) that should respond instantly and mirror the lever state.
 *
 *  ActivatableTargets (IActivatableTarget)
 *  ─────────────────────────────────────────
 *  Timed control: when the lever activates, each entry waits its ActivateDelay,
 *  then calls Activate().  After ActiveDuration seconds (measured from activation,
 *  not from lever press) it calls Deactivate() automatically.  If the lever
 *  deactivates before the delay fires, pending timers are cancelled.  If the
 *  lever deactivates while a target is ON, Deactivate() is called immediately.
 *  Toggling or re-activating the lever cancels all pending work and starts fresh.
 *
 *  Component layout
 *  ────────────────
 *   LeverBase   (StaticMeshComponent, root)
 *   └─ LeverHandle (StaticMeshComponent, child — this rotates)
 *   └─ TriggerVolume (BoxComponent, child — detects player overlap)
 */
UCLASS()
class ALever : public AActor
{
	GENERATED_BODY()

	/** Fixed base of the lever — also the root so actor transform drives both */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* LeverBase;

	/** Animated handle — rotates between HandleOffAngle and HandleOnAngle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* LeverHandle;

	/** Overlap zone that detects the player and triggers activation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerVolume;

public:

	ALever();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Clears all pending activation / deactivation timers so no callbacks fire
	 * after the actor is destroyed or the level ends.
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Called by the editor whenever a property changes.
	 * Keeps TriggerBoxExtent in sync with the actual BoxComponent extent so the
	 * designer sees live feedback without entering PIE.
	 */
	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	// ── Platform targets (ISwitchable — direct, immediate control) ─────────────

	/**
	 * Actors to activate immediately when the lever is pulled.
	 * Each entry must implement ISwitchable; non-implementing actors are skipped.
	 * Use for AMovingPlatform / ARotatingPlatform in SwitchControlled mode.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Targets")
	TArray<TObjectPtr<AActor>> LinkedTargets;

	// ── Activatable targets (IActivatableTarget — timed, delayed control) ──────

	/**
	 * Actors to activate after a configurable delay, then deactivate after a
	 * configurable duration.  Each entry has independent timing settings.
	 * Each actor must implement IActivatableTarget (set up in Blueprint Class
	 * Settings, or inherit IActivatableTarget in C++).
	 *
	 *  Timing flow per entry (lever activates at T=0):
	 *   T = 0              lever activates
	 *   T = ActivateDelay  → Activate() called on this target
	 *   T = ActivateDelay + ActiveDuration  → Deactivate() called automatically
	 *                      (skipped if ActiveDuration == 0 — stays ON indefinitely)
	 *
	 *  When the lever deactivates:
	 *   - Pending Activate timers are cancelled.
	 *   - If the target is already ON, Deactivate() is called immediately.
	 *
	 *  Re-activating (toggle) resets the cycle from scratch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Activatable Targets")
	TArray<FActivatableTargetEntry> ActivatableTargets;

	// ── Behaviour ─────────────────────────────────────────────────────────────

	/**
	 * When true, each player entry flips the lever between on and off.
	 * The lever does not revert when the player walks away.
	 * Takes priority over bOneShot when both flags are set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Behaviour")
	bool bToggleable = false;

	/**
	 * When true, the lever activates once and never deactivates regardless of
	 * whether the player stays inside the trigger.  Ignored if bToggleable is set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Behaviour")
	bool bOneShot = false;

	// ── Handle animation ──────────────────────────────────────────────────────

	/** Axis around which LeverHandle rotates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Animation")
	ELeverAxis LeverAxis = ELeverAxis::Pitch;

	/** Handle relative rotation in degrees when the lever is in the OFF state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Animation")
	float HandleOffAngle = -45.f;

	/** Handle relative rotation in degrees when the lever is in the ON state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Animation")
	float HandleOnAngle = 45.f;

	/**
	 * Time in seconds for the handle to travel the full off↔on arc.
	 * Must be greater than zero.  The same duration is used for both directions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Animation",
		meta = (ClampMin = "0.01"))
	float HandleRotationDuration = 0.4f;

	// ── Visual feedback ───────────────────────────────────────────────────────

	/**
	 * When true, writes a scalar value to a named material parameter on
	 * LeverHandle each time the lever changes state (e.g. an emissive boost).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Visual")
	bool bUseMaterialFeedback = true;

	/** Name of the scalar material parameter to drive on LeverHandle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Visual",
		meta = (EditCondition = "bUseMaterialFeedback"))
	FName ActiveMaterialParameterName = TEXT("EmissiveMultiplier");

	/** Parameter value written while the lever is active (on) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Visual",
		meta = (EditCondition = "bUseMaterialFeedback"))
	float ActiveParameterValue = 5.f;

	/** Parameter value written while the lever is inactive (off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Visual",
		meta = (EditCondition = "bUseMaterialFeedback"))
	float InactiveParameterValue = 0.f;

	/**
	 * When true, enables CustomDepth rendering on LeverHandle while the lever
	 * is active so a stencil-based outline post-process can highlight it.
	 * Requires r.CustomDepth=3 (already set in this project's DefaultEngine.ini).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Visual")
	bool bUseCustomDepthHighlight = false;

	/** Stencil value written to LeverHandle while CustomDepth is active (0-255) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Visual",
		meta = (EditCondition = "bUseCustomDepthHighlight", ClampMin = "0", ClampMax = "255"))
	int32 CustomDepthStencilValue = 1;

	// ── Trigger volume ────────────────────────────────────────────────────────

	/**
	 * Half-extents of the overlap trigger box (XYZ in local space).
	 * Editing this property updates the box immediately in the editor viewport.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Trigger")
	FVector TriggerBoxExtent = FVector(80.f, 80.f, 80.f);

private:

	// ── Lever state ───────────────────────────────────────────────────────────

	/** Whether the lever is currently in the active (on) state */
	bool bIsActive = false;

	/** Prevents a one-shot lever from re-triggering after the first activation */
	bool bHasTriggered = false;

	/** Current toggle state — used only when bToggleable is true */
	bool bToggleState = false;

	/**
	 * Normalised animation progress: 0 = HandleOffAngle, 1 = HandleOnAngle.
	 * Advanced or rewound each tick based on bIsActive.
	 */
	float HandleProgress = 0.f;

	// ── Activatable target runtime state ──────────────────────────────────────

	/**
	 * Parallel to ActivatableTargets — one pair of timer handles + active flag
	 * per entry.  Populated in BeginPlay.  Never serialised or exposed.
	 */
	TArray<FActivatableTimerPair> ActivatableTimers;

	// ── Overlap callbacks ──────────────────────────────────────────────────────

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

	/** Sets active state, notifies ISwitchable targets, schedules activatable
	 *  targets, applies visual feedback. */
	void SetLeverActive(bool bActivate);

	/** Calls SetActivated on every ISwitchable entry in LinkedTargets */
	void SetTargetsActivated(bool bActivate) const;

	/**
	 * Central dispatcher for the activatable target system.
	 *
	 * bActivate = true  — cancels any in-flight timers, deactivates targets that
	 *                     are currently ON, then schedules fresh activation
	 *                     (immediately or after ActivateDelay) per entry.
	 * bActivate = false — cancels pending activation timers; immediately calls
	 *                     Deactivate() on any targets that are currently ON.
	 */
	void ScheduleActivatableTargets(bool bActivate);

	/**
	 * Timer callback: turns one activatable target ON and, if ActiveDuration > 0,
	 * schedules its automatic deactivation.  Bound with the entry Index so each
	 * target has its own independent timer chain.
	 */
	void ActivateTarget(int32 Index);

	/**
	 * Timer callback (or immediate call): turns one activatable target OFF and
	 * resets bTargetIsActive.  Safe to call even if the target is already OFF.
	 */
	void DeactivateTarget(int32 Index);

	/**
	 * Each frame: advances HandleProgress toward the target (0 or 1), then
	 * updates LeverHandle's relative rotation with a SmoothStep ease.
	 */
	void TickHandleAnimation(float DeltaTime);

	/** Writes active/inactive visual state to LeverHandle (material + CustomDepth) */
	void ApplyVisualFeedback(bool bActivate) const;
};

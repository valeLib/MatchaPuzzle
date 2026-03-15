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
	 * 0 = stay ON indefinitely — the target will never auto-deactivate.
	 *
	 * When ActiveDuration is 0, this entry does NOT block lever unlock:
	 * its cycle contribution ends the moment Activate() is called (after the
	 * delay) so the lever cannot be locked forever by a never-expiring target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ClampMin = "0", Units = "s",
			DisplayName = "Active Duration (s)  [0 = indefinite, non-blocking]"))
	float ActiveDuration = 3.f;

	/**
	 * When true, the lever's cycle completing will NOT call Deactivate() on
	 * this target — it stays ON until ActiveDuration expires naturally
	 * (or forever if ActiveDuration is also 0).
	 *
	 * Typical use: a one-way reveal — pull the lever once to permanently show
	 * an object, regardless of the lever's bOneShot or re-pull settings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (DisplayName = "Keep Active When Lever Off"))
	bool bKeepActiveOnLeverOff = false;
};

// ── Internal timer bookkeeping (not a USTRUCT, never exposed) ─────────────────

/**
 *  Holds runtime state for one FActivatableTargetEntry.
 *  Lives in a parallel private array indexed identically to ActivatableTargets.
 */
struct FActivatableTimerPair
{
	/** Fires after ActivateDelay — calls ALever::ActivateTarget(Index). */
	FTimerHandle ActivateTimer;

	/** Fires after ActiveDuration — calls ALever::DeactivateTarget(Index). */
	FTimerHandle DeactivateTimer;

	/** True while this target is in the ON state. */
	bool bTargetIsActive = false;

	/**
	 * True when this entry has finished its contribution to the current cycle
	 * and is no longer blocking lever unlock.
	 *
	 * Set to false when a new cycle starts.
	 * Set to true:
	 *   - When DeactivateTarget fires (ActiveDuration > 0 path).
	 *   - Immediately after Activate() when ActiveDuration == 0 (non-blocking).
	 *   - When the target is invalid or missing (so a bad reference never locks).
	 *
	 * Defaults to true so entries that are skipped at startup don't block.
	 */
	bool bCycleComplete = true;
};

// ── Lever state ───────────────────────────────────────────────────────────────

/**
 *  Internal state of the lever's cycle machine.
 *  Replaces the old fragile boolean combination (bIsActive + bHasTriggered +
 *  bToggleable + bToggleState).
 */
enum class ELeverState : uint8
{
	Idle,         // Ready: a new cycle can be started
	RunningCycle, // Targets working; lever locked against new activations
	Consumed      // One-shot: permanently spent; cannot activate again
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 *  A lever actor that smoothly rotates its handle and activates linked
 *  targets when the player enters its trigger volume.
 *
 *  Cycle-based activation model
 *  ─────────────────────────────
 *  Every pull starts one full action cycle.  The lever locks immediately and
 *  stays locked until every target has finished its work.  The player does not
 *  need to remain inside the trigger — the cycle always runs to completion.
 *
 *  Cycle completion rules:
 *   LinkedTargets (ISwitchable): considered complete when they return true from
 *     ILeverCycleTrackable::IsLeverCycleComplete(), or instantly if they do not
 *     implement ILeverCycleTrackable.
 *   ActivatableTargets: complete when their Deactivate timer fires (ActiveDuration
 *     > 0), or immediately after Activate() fires when ActiveDuration == 0.
 *
 *  Handle visual:
 *   Animates to ON when the cycle starts; returns to OFF when the cycle ends
 *   (non-one-shot).  One-shot levers stay permanently in the ON position.
 *
 *  Two independent target systems operate in parallel:
 *
 *  LinkedTargets (ISwitchable)
 *  ───────────────────────────
 *  Direct activation: SetActivated(true) is called when the cycle starts.
 *  Platforms and similar actors respond immediately; the lever waits for
 *  those that implement ILeverCycleTrackable.
 *
 *  ActivatableTargets (IActivatableTarget)
 *  ─────────────────────────────────────────
 *  Timed activation: each entry waits its ActivateDelay, calls Activate(),
 *  then auto-deactivates after ActiveDuration (if > 0).
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

	// ── Linked targets (ISwitchable — direct activation) ──────────────────────

	/**
	 * Actors that receive SetActivated(true) when the cycle starts.
	 * Each entry must implement ISwitchable; non-implementing actors are skipped.
	 *
	 * If a target also implements ILeverCycleTrackable, the lever waits for
	 * IsLeverCycleComplete() before unlocking.  Targets that don't implement
	 * ILeverCycleTrackable are treated as instantly complete.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Targets")
	TArray<TObjectPtr<AActor>> LinkedTargets;

	// ── Activatable targets (IActivatableTarget — timed activation) ───────────

	/**
	 * Actors activated after a configurable delay and deactivated after a
	 * configurable duration.  Each entry has independent timing settings.
	 *
	 *  Timing flow per entry (cycle starts at T=0):
	 *   T = 0                          cycle starts
	 *   T = ActivateDelay              Activate() called on this target
	 *   T = ActivateDelay+ActiveDur    Deactivate() called automatically
	 *                                  (skipped if ActiveDuration == 0)
	 *
	 *  If ActiveDuration == 0, the target stays ON indefinitely and this entry
	 *  does not block lever unlock (cycle contribution ends right after Activate
	 *  fires).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever|Activatable Targets")
	TArray<FActivatableTargetEntry> ActivatableTargets;

	// ── Behaviour ─────────────────────────────────────────────────────────────

	/**
	 * When true, the lever fires exactly one cycle and then permanently locks
	 * in the ON position (Consumed state).  No further activations are possible.
	 *
	 * When false, the lever returns to Idle after each cycle and can be pulled
	 * again.
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

	/**
	 * Current state of the lever cycle machine.
	 *   Idle         — ready for the next pull
	 *   RunningCycle — targets in progress; new pulls are blocked
	 *   Consumed     — one-shot: permanently locked in the ON position
	 */
	ELeverState LeverState = ELeverState::Idle;

	/**
	 * Normalised animation progress: 0 = HandleOffAngle, 1 = HandleOnAngle.
	 * Advanced toward 1 when a cycle starts, rewound to 0 when it ends
	 * (non-one-shot).  Driven by TickHandleAnimation each frame.
	 */
	float HandleProgress = 0.f;

	// ── Activatable target runtime state ──────────────────────────────────────

	/**
	 * Parallel to ActivatableTargets — one pair of timer handles + state flags
	 * per entry.  Populated in BeginPlay.  Never serialised or exposed.
	 */
	TArray<FActivatableTimerPair> ActivatableTimers;

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

	/** Begins one full action cycle: activates all targets, locks the lever. */
	void StartCycle();

	/**
	 * Called when every target has finished its work for this cycle.
	 * Transitions to Idle (non-one-shot) or Consumed (one-shot).
	 */
	void CompleteCycle();

	/**
	 * Returns true when every linked target and every activatable target entry
	 * has finished its contribution to the current cycle.
	 * Called each tick while LeverState == RunningCycle.
	 */
	bool AreAllTargetsComplete() const;

	/** Polls AreAllTargetsComplete() and calls CompleteCycle() when ready. */
	void TickCycleCompletion(float DeltaTime);

	/** Calls SetActivated(true) on every ISwitchable entry in LinkedTargets. */
	void ActivateLinkedTargets() const;

	/**
	 * Starts the ActivatableTarget timer chain for a new cycle.
	 * Resets bCycleComplete flags, cancels leftover timers, and schedules
	 * fresh Activate() calls (immediately or after ActivateDelay).
	 */
	void StartActivatableTargetCycle();

	/**
	 * Timer callback: calls Activate() on one entry and schedules its
	 * automatic Deactivate() (if ActiveDuration > 0).
	 * Sets bCycleComplete immediately when ActiveDuration == 0.
	 */
	void ActivateTarget(int32 Index);

	/**
	 * Timer callback or direct call: calls Deactivate() on one entry and
	 * sets bCycleComplete = true so the lever knows this entry is done.
	 */
	void DeactivateTarget(int32 Index);

	/**
	 * Each frame: advances HandleProgress toward the target (0 = off, 1 = on)
	 * and updates LeverHandle's relative rotation with a SmoothStep ease.
	 * The target is 1 while RunningCycle or Consumed, 0 while Idle.
	 */
	void TickHandleAnimation(float DeltaTime);

	/** Writes active/inactive visual state to LeverHandle (material + CustomDepth) */
	void ApplyVisualFeedback(bool bActivate) const;
};

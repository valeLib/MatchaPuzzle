// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lever.h"
#include "Switchable.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ALever::ALever()
{
	PrimaryActorTick.bCanEverTick = true;

	// Base is the root so the entire actor transforms as a unit.
	LeverBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverBase"));
	RootComponent = LeverBase;
	LeverBase->SetCollisionProfileName(TEXT("BlockAll"));

	// Handle rotates relative to the base — must be Movable for runtime rotation.
	LeverHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverHandle"));
	LeverHandle->SetupAttachment(LeverBase);
	LeverHandle->SetCollisionProfileName(TEXT("NoCollision"));
	LeverHandle->SetMobility(EComponentMobility::Movable);

	// Trigger zone — overlap detection only, no physical collision.
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(LeverBase);
	TriggerVolume->SetBoxExtent(TriggerBoxExtent);
	TriggerVolume->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	TriggerVolume->SetGenerateOverlapEvents(true);
}

void ALever::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap delegates after reflection is fully initialised.
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(
		this, &ALever::OnTriggerBeginOverlap);
	TriggerVolume->OnComponentEndOverlap.AddDynamic(
		this, &ALever::OnTriggerEndOverlap);

	// Snap the handle to the off angle immediately — no animation on spawn.
	FRotator StartRot = FRotator::ZeroRotator;
	switch (LeverAxis)
	{
	case ELeverAxis::Pitch: StartRot.Pitch = HandleOffAngle; break;
	case ELeverAxis::Roll:  StartRot.Roll  = HandleOffAngle; break;
	case ELeverAxis::Yaw:   StartRot.Yaw   = HandleOffAngle; break;
	}
	LeverHandle->SetRelativeRotation(StartRot);

	ApplyVisualFeedback(false);

	// Pre-size the parallel timer array to match the editor-configured target list.
	// The two arrays are always indexed identically: ActivatableTimers[i] owns the
	// timers and state for ActivatableTargets[i].
	ActivatableTimers.Reset();
	ActivatableTimers.SetNum(ActivatableTargets.Num());
}

void ALever::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Clear every pending timer before the actor is torn down.  Without this,
	// a queued callback could fire on a dangling pointer if the timer manager
	// outlives the actor (e.g. during seamless travel or level unload).
	FTimerManager& TM = GetWorldTimerManager();
	for (FActivatableTimerPair& Pair : ActivatableTimers)
	{
		TM.ClearTimer(Pair.ActivateTimer);
		TM.ClearTimer(Pair.DeactivateTimer);
	}
}

void ALever::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickHandleAnimation(DeltaTime);
}

void ALever::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	TriggerVolume->SetBoxExtent(TriggerBoxExtent);
}

// ── Overlap callbacks ───────────────────────────────────────────────────────────

void ALever::OnTriggerBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor*              OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32                /*OtherBodyIndex*/,
	bool                 /*bFromSweep*/,
	const FHitResult&    /*SweepResult*/)
{
	if (!IsLocalPlayer(OtherActor))
	{
		return;
	}

	// Toggle: flip state on each entry; exit does not revert.
	if (bToggleable)
	{
		bToggleState = !bToggleState;
		SetLeverActive(bToggleState);
		return;
	}

	// One-shot: activate once, ignore subsequent overlaps.
	if (bOneShot && bHasTriggered)
	{
		return;
	}

	bHasTriggered = true;
	SetLeverActive(true);
}

void ALever::OnTriggerEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor*              OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32                /*OtherBodyIndex*/)
{
	if (!IsLocalPlayer(OtherActor))
	{
		return;
	}

	// Toggle and one-shot levers are not reverted when the player leaves.
	if (bToggleable || bOneShot)
	{
		return;
	}

	SetLeverActive(false);
}

// ── Core state change ───────────────────────────────────────────────────────────

bool ALever::IsLocalPlayer(const AActor* OtherActor)
{
	if (!OtherActor)
	{
		return false;
	}
	const APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	return OverlappingPawn && OverlappingPawn->IsPlayerControlled();
}

void ALever::SetLeverActive(bool bActivate)
{
	bIsActive = bActivate;

	// ── System 1: ISwitchable platform targets (unchanged) ────────────────────
	SetTargetsActivated(bActivate);

	// ── System 2: IActivatableTarget timed targets ────────────────────────────
	ScheduleActivatableTargets(bActivate);

	// ── Visual feedback ───────────────────────────────────────────────────────
	ApplyVisualFeedback(bActivate);
}

void ALever::SetTargetsActivated(bool bActivate) const
{
	for (AActor* Target : LinkedTargets)
	{
		if (ISwitchable* Switchable = Cast<ISwitchable>(Target))
		{
			Switchable->SetActivated(bActivate);
		}
	}
}

// ── Activatable target system ───────────────────────────────────────────────────

void ALever::ScheduleActivatableTargets(bool bActivate)
{
	FTimerManager& TM = GetWorldTimerManager();

	for (int32 i = 0; i < ActivatableTargets.Num(); ++i)
	{
		// Guard: ActivatableTimers is sized to match at BeginPlay; this handles
		// the (edge-case) where the array was modified at runtime in Blueprint.
		if (!ActivatableTimers.IsValidIndex(i))
		{
			continue;
		}

		FActivatableTimerPair& Pair = ActivatableTimers[i];

		// ── Cancel all in-flight work for this entry, unconditionally ─────────
		// This is always correct:
		//  - On activation: clears any leftover timers from a previous cycle so
		//    the new cycle starts clean.
		//  - On deactivation: cancels a pending Activate that hasn't fired yet,
		//    satisfying the "cancel pending timers when lever deactivates" requirement.
		TM.ClearTimer(Pair.ActivateTimer);
		TM.ClearTimer(Pair.DeactivateTimer);

		if (bActivate)
		{
			// If the target is already ON and marked to persist, leave it running —
			// its ActiveDuration timer (if any) is already ticking and should not
			// be reset by a re-pull of the lever.
			if (Pair.bTargetIsActive && ActivatableTargets[i].bKeepActiveOnLeverOff)
			{
				continue;
			}

			// If the target is ON from a previous cycle (and not persistent),
			// deactivate it immediately so the new cycle starts from a clean OFF state.
			if (Pair.bTargetIsActive)
			{
				DeactivateTarget(i);
			}

			const float Delay = ActivatableTargets[i].ActivateDelay;

			if (Delay > 0.f)
			{
				// Schedule delayed activation via a bound-index delegate so each
				// entry has its own independent timer — no Tick logic required.
				FTimerDelegate Del;
				Del.BindUObject(this, &ALever::ActivateTarget, i);
				TM.SetTimer(Pair.ActivateTimer, Del, Delay, /*bLoop=*/ false);
			}
			else
			{
				// Zero delay: activate immediately this frame.
				ActivateTarget(i);
			}
		}
		else // lever deactivating
		{
			// Pending activation was already cancelled above (ClearTimer).
			// Only turn the target OFF if it hasn't been marked to persist
			// beyond the lever's lifetime.
			if (Pair.bTargetIsActive && !ActivatableTargets[i].bKeepActiveOnLeverOff)
			{
				DeactivateTarget(i);
			}
		}
	}
}

void ALever::ActivateTarget(int32 Index)
{
	if (!ActivatableTargets.IsValidIndex(Index) || !ActivatableTimers.IsValidIndex(Index))
	{
		return;
	}

	AActor* Target = ActivatableTargets[Index].Target.Get();
	if (!IsValid(Target))
	{
		return;
	}

	// Execute_Activate dispatches to the Blueprint event or C++ _Implementation,
	// whichever the actor provides.  Direct vtable calls (->Activate()) only reach
	// C++ overrides and miss Blueprint graph overrides.
	if (Target->GetClass()->ImplementsInterface(UActivatableTarget::StaticClass()))
	{
		IActivatableTarget::Execute_Activate(Target);
		ActivatableTimers[Index].bTargetIsActive = true;

		// Schedule automatic deactivation after ActiveDuration.
		// If ActiveDuration is 0, the target stays ON until the lever deactivates.
		const float Duration = ActivatableTargets[Index].ActiveDuration;
		if (Duration > 0.f)
		{
			FTimerDelegate Del;
			Del.BindUObject(this, &ALever::DeactivateTarget, Index);
			GetWorldTimerManager().SetTimer(
				ActivatableTimers[Index].DeactivateTimer, Del, Duration, /*bLoop=*/ false);
		}
	}
}

void ALever::DeactivateTarget(int32 Index)
{
	if (!ActivatableTargets.IsValidIndex(Index) || !ActivatableTimers.IsValidIndex(Index))
	{
		return;
	}

	// Clear the deactivation timer first — this function may be called either by
	// the timer firing naturally OR by ScheduleActivatableTargets cancelling early.
	// ClearTimer on an already-fired or invalid handle is always a safe no-op.
	GetWorldTimerManager().ClearTimer(ActivatableTimers[Index].DeactivateTimer);

	ActivatableTimers[Index].bTargetIsActive = false;

	AActor* Target = ActivatableTargets[Index].Target.Get();
	if (!IsValid(Target))
	{
		return;
	}

	if (Target->GetClass()->ImplementsInterface(UActivatableTarget::StaticClass()))
	{
		IActivatableTarget::Execute_Deactivate(Target);
	}
}

// ── Handle animation ────────────────────────────────────────────────────────────

void ALever::TickHandleAnimation(float DeltaTime)
{
	if (HandleRotationDuration <= 0.f)
	{
		return;
	}

	const bool bShouldAdvance = bIsActive  && HandleProgress < 1.f;
	const bool bShouldRewind  = !bIsActive && HandleProgress > 0.f;

	if (!bShouldAdvance && !bShouldRewind)
	{
		return;
	}

	const float ProgressRate = DeltaTime / HandleRotationDuration;

	if (bShouldAdvance)
	{
		HandleProgress = FMath::Clamp(HandleProgress + ProgressRate, 0.f, 1.f);
	}
	else
	{
		HandleProgress = FMath::Clamp(HandleProgress - ProgressRate, 0.f, 1.f);
	}

	const float Smoothed = FMath::SmoothStep(0.f, 1.f, HandleProgress);
	const float Angle    = FMath::Lerp(HandleOffAngle, HandleOnAngle, Smoothed);

	FRotator NewRot = FRotator::ZeroRotator;
	switch (LeverAxis)
	{
	case ELeverAxis::Pitch: NewRot.Pitch = Angle; break;
	case ELeverAxis::Roll:  NewRot.Roll  = Angle; break;
	case ELeverAxis::Yaw:   NewRot.Yaw   = Angle; break;
	}

	LeverHandle->SetRelativeRotation(NewRot);
}

void ALever::ApplyVisualFeedback(bool bActivate) const
{
	if (bUseMaterialFeedback)
	{
		const float ParamValue = bActivate ? ActiveParameterValue : InactiveParameterValue;
		LeverHandle->SetScalarParameterValueOnMaterials(ActiveMaterialParameterName, ParamValue);
	}

	if (bUseCustomDepthHighlight)
	{
		LeverHandle->SetRenderCustomDepth(bActivate);
		if (bActivate)
		{
			LeverHandle->SetCustomDepthStencilValue(CustomDepthStencilValue);
		}
	}
}

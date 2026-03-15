// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lever.h"
#include "LeverCycleTrackable.h"
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
	// ActivatableTimers[i] owns the timers and state for ActivatableTargets[i].
	ActivatableTimers.Reset();
	ActivatableTimers.SetNum(ActivatableTargets.Num());
}

void ALever::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Cancel every pending timer before the actor is torn down.  Without this,
	// a queued callback could fire on a dangling pointer during level unload.
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
	TickCycleCompletion(DeltaTime);
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

	// Only Idle allows a new cycle.  RunningCycle = busy lock, Consumed = spent.
	if (LeverState != ELeverState::Idle)
	{
		return;
	}

	StartCycle();
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

	// The cycle runs to completion regardless of whether the player stays.
	// Nothing to do here — TickCycleCompletion drives the unlock automatically.
}

// ── Cycle control ───────────────────────────────────────────────────────────────

bool ALever::IsLocalPlayer(const AActor* OtherActor)
{
	if (!OtherActor)
	{
		return false;
	}
	const APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	return OverlappingPawn && OverlappingPawn->IsPlayerControlled();
}

void ALever::StartCycle()
{
	LeverState = ELeverState::RunningCycle;

	ActivateLinkedTargets();
	StartActivatableTargetCycle();
	ApplyVisualFeedback(true);
	// HandleProgress animates toward 1 automatically via TickHandleAnimation.
}

void ALever::CompleteCycle()
{
	if (bOneShot)
	{
		// Permanently spent — handle stays in the ON position, visual stays active.
		LeverState = ELeverState::Consumed;
	}
	else
	{
		// Return to ready — handle animates back to OFF, visual deactivates.
		LeverState = ELeverState::Idle;
		ApplyVisualFeedback(false);
		// HandleProgress rewinds to 0 automatically via TickHandleAnimation.
	}
}

void ALever::TickCycleCompletion(float /*DeltaTime*/)
{
	if (LeverState != ELeverState::RunningCycle)
	{
		return;
	}

	if (AreAllTargetsComplete())
	{
		CompleteCycle();
	}
}

bool ALever::AreAllTargetsComplete() const
{
	// Check each linked target that knows how to report its completion.
	// Targets without ILeverCycleTrackable are treated as instantly done.
	for (const TObjectPtr<AActor>& Target : LinkedTargets)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		const ILeverCycleTrackable* Trackable = Cast<ILeverCycleTrackable>(Target.Get());
		if (Trackable && !Trackable->IsLeverCycleComplete())
		{
			return false;
		}
	}

	// Check each activatable target's bCycleComplete flag.
	for (const FActivatableTimerPair& Pair : ActivatableTimers)
	{
		if (!Pair.bCycleComplete)
		{
			return false;
		}
	}

	return true;
}

// ── Linked target activation ────────────────────────────────────────────────────

void ALever::ActivateLinkedTargets() const
{
	for (const TObjectPtr<AActor>& Target : LinkedTargets)
	{
		if (ISwitchable* Switchable = Cast<ISwitchable>(Target.Get()))
		{
			Switchable->SetActivated(true);
		}
	}
}

// ── Activatable target system ───────────────────────────────────────────────────

void ALever::StartActivatableTargetCycle()
{
	FTimerManager& TM = GetWorldTimerManager();

	for (int32 i = 0; i < ActivatableTargets.Num(); ++i)
	{
		if (!ActivatableTimers.IsValidIndex(i))
		{
			continue;
		}

		FActivatableTimerPair& Pair = ActivatableTimers[i];

		// Cancel any in-flight timers from a previous cycle before starting fresh.
		TM.ClearTimer(Pair.ActivateTimer);
		TM.ClearTimer(Pair.DeactivateTimer);

		// Defensively deactivate any target left ON from a previous cycle.
		// This should not occur in normal play since cycles run to completion,
		// but guards against edge cases (level reload, rapid re-entry, etc.).
		if (Pair.bTargetIsActive && !ActivatableTargets[i].bKeepActiveOnLeverOff)
		{
			DeactivateTarget(i);
		}

		// Mark this entry as busy for the new cycle.  Must happen after the
		// defensive DeactivateTarget above (which sets bCycleComplete = true)
		// so the new cycle's false value is the final write.
		Pair.bCycleComplete = false;

		const float Delay = ActivatableTargets[i].ActivateDelay;
		if (Delay > 0.f)
		{
			FTimerDelegate Del;
			Del.BindUObject(this, &ALever::ActivateTarget, i);
			TM.SetTimer(Pair.ActivateTimer, Del, Delay, /*bLoop=*/ false);
		}
		else
		{
			ActivateTarget(i);
		}
	}
}

void ALever::ActivateTarget(int32 Index)
{
	if (!ActivatableTargets.IsValidIndex(Index) || !ActivatableTimers.IsValidIndex(Index))
	{
		return;
	}

	FActivatableTimerPair& Pair = ActivatableTimers[Index];

	AActor* Target = ActivatableTargets[Index].Target.Get();
	if (!IsValid(Target))
	{
		// Target is gone — mark complete so a bad reference never locks the lever.
		Pair.bCycleComplete = true;
		return;
	}

	if (!Target->GetClass()->ImplementsInterface(UActivatableTarget::StaticClass()))
	{
		// Missing interface — not a blocking entry.
		Pair.bCycleComplete = true;
		return;
	}

	IActivatableTarget::Execute_Activate(Target);
	Pair.bTargetIsActive = true;

	const float Duration = ActivatableTargets[Index].ActiveDuration;
	if (Duration > 0.f)
	{
		// Schedule automatic deactivation.  bCycleComplete is set true
		// when DeactivateTarget fires, which lets the lever know this entry is done.
		FTimerDelegate Del;
		Del.BindUObject(this, &ALever::DeactivateTarget, Index);
		GetWorldTimerManager().SetTimer(Pair.DeactivateTimer, Del, Duration, /*bLoop=*/ false);
	}
	else
	{
		// ActiveDuration == 0: target stays ON indefinitely with no auto-deactivation.
		// This entry's contribution to the cycle ends immediately so the lever
		// cannot be locked forever by a target that never deactivates.
		Pair.bCycleComplete = true;
	}
}

void ALever::DeactivateTarget(int32 Index)
{
	if (!ActivatableTargets.IsValidIndex(Index) || !ActivatableTimers.IsValidIndex(Index))
	{
		return;
	}

	FActivatableTimerPair& Pair = ActivatableTimers[Index];

	// ClearTimer is safe to call on an already-fired or invalid handle.
	GetWorldTimerManager().ClearTimer(Pair.DeactivateTimer);

	Pair.bTargetIsActive = false;
	Pair.bCycleComplete  = true;   // This entry's cycle contribution is done.

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

	// Handle advances toward 1 (ON) while active or consumed, rewinds to 0 (OFF)
	// only when Idle.  This means one-shot levers stay permanently in the ON pose.
	const bool bLeverOn = (LeverState != ELeverState::Idle);

	const bool bShouldAdvance = bLeverOn  && HandleProgress < 1.f;
	const bool bShouldRewind  = !bLeverOn && HandleProgress > 0.f;

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

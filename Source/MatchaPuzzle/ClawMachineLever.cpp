// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClawMachineLever.h"
#include "ClawControllable.h"
#include "PuzzlePlayerController.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"

AClawMachineLever::AClawMachineLever()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Sphere as root so the actor's world transform drives the interaction zone.
	InteractionTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionTrigger"));
	RootComponent = InteractionTrigger;

	InteractionTrigger->InitSphereRadius(150.f);
	InteractionTrigger->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	InteractionTrigger->SetGenerateOverlapEvents(true);

	// Fixed base — assign a mesh in the Details panel.
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(RootComponent);
	BaseMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// Lever handle — rotates to reflect horizontal and vertical control values.
	LeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverMesh"));
	LeverMesh->SetupAttachment(RootComponent);
	LeverMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// Interaction prompt — shown above the lever when the player is nearby.
	InteractPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractPromptWidget"));
	InteractPromptWidget->SetupAttachment(RootComponent);
	InteractPromptWidget->SetRelativeLocation(PromptOffset);
	InteractPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractPromptWidget->SetDrawSize(FVector2D(200.f, 60.f));
	InteractPromptWidget->SetVisibility(false);
	InteractPromptWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AClawMachineLever::BeginPlay()
{
	Super::BeginPlay();

	InteractionTrigger->OnComponentBeginOverlap.AddDynamic(
		this, &AClawMachineLever::OnTriggerBeginOverlap);

	InteractionTrigger->OnComponentEndOverlap.AddDynamic(
		this, &AClawMachineLever::OnTriggerEndOverlap);

	// Capture the editor-placed rotation once as the neutral rest position.
	// This is the only time InitialLeverRotation is written.
	if (LeverMesh)
	{
		InitialLeverRotation = LeverMesh->GetRelativeRotation();

		if (bUseHoverMaterial)
		{
			if (UMaterialInterface* BaseMat = LeverMesh->GetMaterial(0))
			{
				LeverMaterialInstance = UMaterialInstanceDynamic::Create(BaseMat, this);
				LeverMesh->SetMaterial(0, LeverMaterialInstance);
			}
		}
	}

	// Bind the widget class and apply the prompt offset now that the world exists.
	if (InteractPromptWidget)
	{
		InteractPromptWidget->SetRelativeLocation(PromptOffset);

		if (InteractPromptWidgetClass)
		{
			InteractPromptWidget->SetWidgetClass(InteractPromptWidgetClass);
		}
	}

	// Clamp starting values so the lever cannot begin in an invalid state.
	// VerticalValue starts at VerticalMax (0 = neutral upright position).
	HorizontalValue = FMath::Clamp(HorizontalValue, HorizontalMin, HorizontalMax);
	VerticalValue   = FMath::Clamp(VerticalValue,   VerticalMin,   VerticalMax);

	// Sync visual and material to the initial state immediately.
	SetHoverState(false);
	SetControlledState(false);

	if (LeverMesh)
	{
		LeverMesh->SetRelativeRotation(ComputeTargetRotation());
	}

	// Push starting values to the target so it begins at the correct position.
	SendInputToTarget();
}

void AClawMachineLever::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Input is no longer polled here.  APuzzlePlayerController calls
	// AddHorizontalInput / AddVerticalInput each frame while the lever is active.

	// ── Visual animation ──────────────────────────────────────────────────────
	// Target rotation is computed from the stored control values, never from
	// the current mesh rotation, so there is no cumulative drift.

	if (!LeverMesh)
	{
		return;
	}

	const FRotator TargetRot = ComputeTargetRotation();

	if (bSmoothMotion)
	{
		const FRotator Current = LeverMesh->GetRelativeRotation();
		if (!Current.Equals(TargetRot, 0.1f))
		{
			LeverMesh->SetRelativeRotation(
				FMath::RInterpTo(Current, TargetRot, DeltaSeconds, LeverMoveSpeed));
		}
	}
	else
	{
		LeverMesh->SetRelativeRotation(TargetRot);
	}
}

// ── Controller-facing API ──────────────────────────────────────────────────────

void AClawMachineLever::BeginControl(APuzzlePlayerController* Controller)
{
	bIsControlled  = true;
	ActiveController = Controller;
	SetControlledState(true);
	SetPromptVisible(false);
}

void AClawMachineLever::EndControl()
{
	bIsControlled = false;
	SetControlledState(false);

	// Notify the controller so it can clear its ActiveControlledLever reference.
	// This covers the case where EndControl is triggered internally (e.g. player
	// exits the trigger) rather than by a direct controller button press.
	if (APuzzlePlayerController* PC = ActiveController.Get())
	{
		PC->OnLeverControlEnded(this);
	}

	ActiveController.Reset();

	// If the player is still standing in the trigger, re-show the prompt so
	// they know they can grab the lever again.
	SetPromptVisible(bPlayerInRange);
}

void AClawMachineLever::AddHorizontalInput(float Value, float DeltaTime)
{
	if (!bIsControlled || FMath::IsNearlyZero(Value))
	{
		return;
	}

	HorizontalValue = FMath::Clamp(
		HorizontalValue + Value * InputStep * DeltaTime,
		HorizontalMin, HorizontalMax);

	SendInputToTarget();
}

void AClawMachineLever::AddVerticalInput(float Value, float DeltaTime)
{
	if (!bIsControlled || FMath::IsNearlyZero(Value))
	{
		return;
	}

	// VerticalMax is the neutral position (0).  FMath::Clamp handles both
	// directions: W (positive Value) is clamped at VerticalMax so the lever
	// cannot rise above neutral; S (negative Value) is clamped at VerticalMin.
	VerticalValue = FMath::Clamp(
		VerticalValue + Value * InputStep * DeltaTime,
		VerticalMin, VerticalMax);

	SendInputToTarget();
}

// ── Overlap callbacks ──────────────────────────────────────────────────────────

void AClawMachineLever::OnTriggerBeginOverlap(
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

	bPlayerInRange = true;

	// Register with the controller so it knows this lever can be activated.
	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (APuzzlePlayerController* PC = Pawn->GetController<APuzzlePlayerController>())
		{
			PC->SetCurrentNearbyLever(this);
		}
	}

	SetHoverState(true);
	SetPromptVisible(true);
}

void AClawMachineLever::OnTriggerEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor*              OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32                /*OtherBodyIndex*/)
{
	if (!IsLocalPlayer(OtherActor))
	{
		return;
	}

	// If the player walks away while controlling the lever, end control cleanly.
	// EndControl notifies the controller, so ActiveControlledLever is cleared there.
	if (bIsControlled)
	{
		EndControl();
	}

	bPlayerInRange = false;

	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (APuzzlePlayerController* PC = Pawn->GetController<APuzzlePlayerController>())
		{
			PC->ClearCurrentNearbyLever(this);
		}
	}

	SetHoverState(false);
	SetPromptVisible(false);
}

// ── Helpers ────────────────────────────────────────────────────────────────────

bool AClawMachineLever::IsLocalPlayer(const AActor* OtherActor)
{
	if (!OtherActor)
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	return Pawn && Pawn->IsPlayerControlled();
}

void AClawMachineLever::SetHoverState(bool bHovered)
{
	if (!LeverMesh)
	{
		return;
	}

	// Material parameter — safe no-op if the parameter name does not exist.
	if (bUseHoverMaterial && LeverMaterialInstance)
	{
		LeverMaterialInstance->SetScalarParameterValue(
			TEXT("IsHovered"), bHovered ? 1.0f : 0.0f);
	}

	// CustomDepth outline — only writes to LeverMesh, never BaseMesh.
	if (bUseCustomDepth)
	{
		LeverMesh->SetRenderCustomDepth(bHovered);
		if (bHovered)
		{
			LeverMesh->SetCustomDepthStencilValue(CustomDepthStencilValue);
		}
	}
}

void AClawMachineLever::SetControlledState(bool bControlled)
{
	// IsControlled is a separate scalar parameter on the same DMI.
	// Safe no-op if the parameter does not exist in the material.
	if (bUseHoverMaterial && LeverMaterialInstance)
	{
		LeverMaterialInstance->SetScalarParameterValue(
			TEXT("IsControlled"), bControlled ? 1.0f : 0.0f);
	}
}

FRotator AClawMachineLever::ComputeTargetRotation() const
{
	// Map HorizontalValue from [HorizontalMin, HorizontalMax] to [-1, 1].
	// The centre of the range maps to 0° so a symmetric range like [-1, 1]
	// produces no lean at HorizontalValue = 0.
	const float HRange = HorizontalMax - HorizontalMin;
	const float NormH  = (HRange > KINDA_SMALL_NUMBER)
		? (HorizontalValue - HorizontalMin) / HRange * 2.f - 1.f
		: 0.f;

	// Map VerticalValue from [VerticalMin, VerticalMax] to [0, 1].
	// VerticalMax (neutral = 0) maps to 0° lean.
	// VerticalMin (fully down) maps to LeverDownAngleRange.
	// Formula: NormV = (VerticalMax - VerticalValue) / (VerticalMax - VerticalMin)
	const float VRange = VerticalMax - VerticalMin;   // e.g. 0 - (-1) = 1
	const float NormV  = (VRange > KINDA_SMALL_NUMBER)
		? (VerticalMax - VerticalValue) / VRange
		: 0.f;

	// Offset from the rest rotation captured at BeginPlay.
	// Roll  → left / right lean (rotation around local X axis)
	// Pitch → forward / down lean (rotation around local Y axis)
	FRotator Target = InitialLeverRotation;
	Target.Roll  += NormH * LeverHorizontalAngleRange;
	Target.Pitch += NormV * LeverDownAngleRange;

	return Target;
}

void AClawMachineLever::SetPromptVisible(bool bVisible)
{
	if (InteractPromptWidget)
	{
		InteractPromptWidget->SetVisibility(bVisible);
	}
}

void AClawMachineLever::SendInputToTarget() const
{
	if (!ControlledTarget)
	{
		return;
	}

	if (ControlledTarget->Implements<UClawControllable>())
	{
		IClawControllable::Execute_SetLeverInput(
			ControlledTarget, HorizontalValue, VerticalValue);
	}
}

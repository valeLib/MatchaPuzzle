// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClawMachineLever.h"
#include "ClawControllable.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
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
	// All visual computation offsets from this value, so the lever never drifts.
	if (LeverMesh)
	{
		InitialLeverRotation = LeverMesh->GetRelativeRotation();

		// Create a Dynamic Material Instance for hover feedback.
		// BaseMesh materials are never touched.
		if (bUseHoverMaterial)
		{
			if (UMaterialInterface* BaseMat = LeverMesh->GetMaterial(0))
			{
				LeverMaterialInstance = UMaterialInstanceDynamic::Create(BaseMat, this);
				LeverMesh->SetMaterial(0, LeverMaterialInstance);
			}
		}
	}

	// Clamp starting values so the lever cannot begin in an invalid state.
	HorizontalValue = FMath::Clamp(HorizontalValue, HorizontalMin, HorizontalMax);
	VerticalValue   = FMath::Clamp(VerticalValue,   VerticalMin,   VerticalMax);

	// Sync visual and hover to the initial state immediately.
	SetHoverState(false);

	if (LeverMesh)
	{
		LeverMesh->SetRelativeRotation(ComputeTargetRotation());
	}

	// Push starting values to the target so it begins in the correct position.
	SendInputToTarget();
}

void AClawMachineLever::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ── Input polling ─────────────────────────────────────────────────────────
	// Keys are polled directly on the PlayerController rather than using input
	// binding, so no EnableInput / DisableInput management is needed.
	// All input is ignored while bPlayerInRange is false.

	if (bPlayerInRange)
	{
		if (APlayerController* PC = InteractingController.Get())
		{
			float DH = 0.f;
			float DV = 0.f;

			if (PC->IsInputKeyDown(EKeys::E)) DH += InputStep;
			if (PC->IsInputKeyDown(EKeys::Q)) DH -= InputStep;
			if (PC->IsInputKeyDown(EKeys::R)) DV += InputStep;

			if (!FMath::IsNearlyZero(DH) || !FMath::IsNearlyZero(DV))
			{
				HorizontalValue = FMath::Clamp(
					HorizontalValue + DH * DeltaSeconds, HorizontalMin, HorizontalMax);
				VerticalValue = FMath::Clamp(
					VerticalValue   + DV * DeltaSeconds, VerticalMin, VerticalMax);

				SendInputToTarget();
			}
		}
	}

	// ── Visual animation ──────────────────────────────────────────────────────
	// Target rotation is computed fresh from the current control values.
	// It is NOT derived from the current mesh rotation, so there is no
	// cumulative offset and the lever cannot drift over time.

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

	bPlayerInRange         = true;
	InteractingController  = GetPlayerController(OtherActor);
	SetHoverState(true);
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

	bPlayerInRange = false;
	InteractingController.Reset();
	SetHoverState(false);
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

APlayerController* AClawMachineLever::GetPlayerController(const AActor* PlayerActor)
{
	if (!PlayerActor)
	{
		return nullptr;
	}

	const APawn* Pawn = Cast<APawn>(PlayerActor);
	return Pawn ? Pawn->GetController<APlayerController>() : nullptr;
}

void AClawMachineLever::SetHoverState(bool bHovered)
{
	if (!LeverMesh)
	{
		return;
	}

	// Material parameter — safe no-op if parameter name doesn't exist.
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

FRotator AClawMachineLever::ComputeTargetRotation() const
{
	// Map HorizontalValue from [HorizontalMin, HorizontalMax] to [-1, 1].
	// Centre of the range maps to 0, so a symmetric range like [-1, 1] has
	// a neutral midpoint at 0.
	const float HRange = HorizontalMax - HorizontalMin;
	const float NormH  = (HRange > KINDA_SMALL_NUMBER)
		? (HorizontalValue - HorizontalMin) / HRange * 2.f - 1.f
		: 0.f;

	// Map VerticalValue from [VerticalMin, VerticalMax] to [0, 1].
	// VerticalMin = lever at rest (0), VerticalMax = fully pushed down (1).
	const float VRange = VerticalMax - VerticalMin;
	const float NormV  = (VRange > KINDA_SMALL_NUMBER)
		? (VerticalValue - VerticalMin) / VRange
		: 0.f;

	// Offset from the rest rotation captured at BeginPlay.
	// Roll  → left / right lean  (rotation around local X axis)
	// Pitch → forward / down lean (rotation around local Y axis)
	// Swap these two lines if your mesh's local axes are oriented differently.
	FRotator Target = InitialLeverRotation;
	Target.Roll  += NormH * LeverHorizontalAngleRange;
	Target.Pitch += NormV * LeverDownAngleRange;

	return Target;
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

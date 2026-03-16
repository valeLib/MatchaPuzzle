// Copyright Epic Games, Inc. All Rights Reserved.

#include "BillboardIconActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

ABillboardIconActor::ABillboardIconActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// ── Root ────────────────────────────────────────────────────────────────
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// ── Icon quad ────────────────────────────────────────────────────────────
	IconMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IconMesh"));
	IconMesh->SetupAttachment(SceneRoot);

	// Icons are visual-only: no collision, no shadow cast.
	IconMesh->SetCollisionProfileName(TEXT("NoCollision"));
	IconMesh->SetCastShadow(false);
	IconMesh->SetMobility(EComponentMobility::Movable);

	// /Engine/BasicShapes/Plane is always available — it is a 100×100 cm flat
	// quad lying in the XY plane with its surface normal pointing in +Z.
	// The billboard rotation drives +Z to face the camera each frame.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneFinder.Succeeded())
	{
		IconMesh->SetStaticMesh(PlaneFinder.Object);
	}

	// Default material — the project's dedicated icon material.
	// Using ConstructorHelpers so the reference is cooked correctly.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/_Project/Art/Materials/M_SwitchIcon"));
	if (MatFinder.Succeeded())
	{
		IconMaterial = MatFinder.Object;
		IconMesh->SetMaterial(0, IconMaterial);
	}
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

void ABillboardIconActor::BeginPlay()
{
	Super::BeginPlay();

	// Apply transform first so the mesh is in the right place before the
	// material instance is created (avoids a one-frame visual glitch).
	ApplyTransform();
	InitDynMaterial();
	ApplyMaterialParameters();
	UpdateVisibility();
}

void ABillboardIconActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateBillboard();
}

void ABillboardIconActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Called in the editor on every property change — gives live viewport
	// feedback without entering PIE.
	ApplyTransform();
	InitDynMaterial();
	ApplyMaterialParameters();
	UpdateVisibility();
}

// ── Blueprint-callable API ─────────────────────────────────────────────────────

void ABillboardIconActor::SetActive(bool bActive)
{
	bIsActive = bActive;
	UpdateVisibility();
}

void ABillboardIconActor::SetIconTexture(UTexture2D* NewTexture)
{
	IconTexture = NewTexture;
	ApplyMaterialParameters();
}

void ABillboardIconActor::SetTintColor(FLinearColor NewColor)
{
	TintColor = NewColor;
	ApplyMaterialParameters();
}

void ABillboardIconActor::SetOpacity(float NewOpacity)
{
	Opacity = FMath::Clamp(NewOpacity, 0.f, 1.f);
	ApplyMaterialParameters();
}

void ABillboardIconActor::SetWorldScale(float NewScale)
{
	WorldScale = FMath::Max(NewScale, 0.001f);
	ApplyTransform();
}

void ABillboardIconActor::SetRelativeOffset(FVector NewOffset)
{
	RelativeOffset = NewOffset;
	ApplyTransform();
}

// ── Private helpers ────────────────────────────────────────────────────────────

void ABillboardIconActor::InitDynMaterial()
{
	if (!IconMesh || !IconMaterial)
	{
		return;
	}

	// Reuse the existing DMI if it already wraps the current IconMaterial.
	// This prevents DMI-of-DMI chaining across repeated OnConstruction calls.
	UMaterialInstanceDynamic* Existing =
		Cast<UMaterialInstanceDynamic>(IconMesh->GetMaterial(0));

	if (Existing && Existing->Parent == IconMaterial)
	{
		DynMaterial = Existing;
		return;
	}

	// IconMaterial changed (or first call) — create a fresh DMI.
	DynMaterial = UMaterialInstanceDynamic::Create(IconMaterial, this);
	IconMesh->SetMaterial(0, DynMaterial);
}

void ABillboardIconActor::ApplyMaterialParameters() const
{
	if (!DynMaterial)
	{
		return;
	}

	// Texture — silently skipped if the parameter doesn't exist in the material.
	if (IconTexture && !TextureParameterName.IsNone())
	{
		DynMaterial->SetTextureParameterValue(TextureParameterName, IconTexture);
	}

	if (!ColorParameterName.IsNone())
	{
		DynMaterial->SetVectorParameterValue(ColorParameterName, TintColor);
	}

	if (!OpacityParameterName.IsNone())
	{
		DynMaterial->SetScalarParameterValue(OpacityParameterName, Opacity);
	}
}

void ABillboardIconActor::UpdateVisibility() const
{
	if (!IconMesh)
	{
		return;
	}

	// When bHiddenWhenDisabled is false, visibility is never touched here.
	// The caller can instead drive Opacity to 0 for a fade effect.
	if (bHiddenWhenDisabled)
	{
		IconMesh->SetVisibility(bIsActive);
	}
}

void ABillboardIconActor::ApplyTransform() const
{
	if (!IconMesh)
	{
		return;
	}

	// Offset from the actor root — positive Z floats the icon above the switch.
	IconMesh->SetRelativeLocation(RelativeOffset);

	// Uniform scale.  The engine plane is 100×100 cm at scale 1.0.
	IconMesh->SetRelativeScale3D(FVector(WorldScale));
}

void ABillboardIconActor::UpdateBillboard() const
{
	if (!bEnableBillboard || !IconMesh)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// GetFirstPlayerController is appropriate for single-player puzzle games.
	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector MeshLocation   = IconMesh->GetComponentLocation();

	// Direction from the icon to the camera — this is what +Z should point toward.
	FVector LookDir = (CameraLocation - MeshLocation).GetSafeNormal();

	if (bYawOnly)
	{
		// Project onto the horizontal plane so the icon stays upright.
		LookDir.Z = 0.f;

		// If the camera is directly above or below (Z was the dominant component),
		// the projected vector is near-zero — bail out to avoid a degenerate rotation.
		if (!LookDir.Normalize())
		{
			return;
		}
	}

	// When the look direction is nearly parallel to world up (camera directly
	// overhead in full-billboard mode), the up vector degenerates.  Fall back
	// to world forward so the icon stays stable.
	const FVector SafeUp = (FMath::Abs(LookDir | FVector::UpVector) > 0.99f)
		? FVector::ForwardVector
		: FVector::UpVector;

	// MakeFromZX(InZ, InX):
	//   Z axis  = LookDir  → the plane's +Z normal faces the camera.
	//   X axis  ≈ SafeUp   → keeps the icon's "up" edge oriented toward world up.
	//   Y axis  = Z × X    → right edge, derived automatically.
	const FRotator NewRot = FRotationMatrix::MakeFromZX(LookDir, SafeUp).Rotator();

	// Set world rotation directly so the result is camera-relative regardless
	// of how this actor is attached or what its parent's rotation is.
	IconMesh->SetWorldRotation(NewRot);
}

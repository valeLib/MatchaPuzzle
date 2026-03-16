// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BillboardIconActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

/**
 *  A lightweight, reusable billboard actor for displaying a 3D icon in world space.
 *
 *  Designed to be placed above buttons, floor switches, levers, or any
 *  interactable.  Each frame the icon quad rotates so its face points directly
 *  at the player camera (billboard behaviour), eliminating the need for any
 *  manual rotation logic in Blueprint.
 *
 *  Typical setup
 *  ─────────────
 *  1.  Attach ABillboardIconActor as a child actor to a FloorSwitch (or any actor).
 *  2.  Assign IconTexture in the Details panel.
 *  3.  Call SetActive(false) from Blueprint or C++ when the switch is consumed.
 *
 *  Component hierarchy
 *  ───────────────────
 *   SceneRoot  (USceneComponent — keeps actor pivot clean)
 *   └─ IconMesh (UStaticMeshComponent — a 100 × 100 cm flat plane)
 *
 *  Billboard rotation
 *  ──────────────────
 *  The plane mesh uses the +Z direction as its surface normal.
 *  Each Tick, the component's world rotation is set so that +Z faces the player
 *  camera.  bYawOnly constrains this to the horizontal plane so the icon stays
 *  perfectly upright.
 *
 *  Material contract  (default: M_SwitchIcon)
 *  ──────────────────────────────────────────
 *  The material assigned to IconMaterial must expose three parameters
 *  (names are configurable via the Icon|Material category):
 *
 *    Texture2D parameter → TextureParameterName  (default "IconTexture")
 *    Vector    parameter → ColorParameterName    (default "TintColor")
 *    Scalar    parameter → OpacityParameterName  (default "Opacity")
 *
 *  A UMaterialInstanceDynamic is created at runtime wrapping IconMaterial so
 *  parameters can be pushed without touching the shared material asset.
 */
UCLASS()
class ABillboardIconActor : public AActor
{
	GENERATED_BODY()

	/** Keeps the actor pivot separate from the billboard mesh rotation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	USceneComponent* SceneRoot;

	/**
	 *  Flat plane quad (100×100 cm, +Z normal) that displays the icon texture.
	 *  Billboard logic rotates this component every frame so +Z faces the camera.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* IconMesh;

public:

	ABillboardIconActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 *  Called by the editor when a property changes.
	 *  Rebuilds the dynamic material and pushes the current property values so
	 *  the viewport reflects edits without entering PIE.
	 */
	virtual void OnConstruction(const FTransform& Transform) override;

	// ── Appearance ────────────────────────────────────────────────────────────

	/**
	 *  The base material to wrap with a dynamic instance.
	 *  Must expose the three parameters listed in the class comment.
	 *  Defaults to M_SwitchIcon (set in the constructor).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Appearance")
	TObjectPtr<UMaterialInterface> IconMaterial = nullptr;

	/**
	 *  Texture assigned to the TextureParameterName slot of the dynamic material.
	 *  Swap at runtime with SetIconTexture().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Appearance")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	/** Multiplicative tint applied to the texture in the material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Appearance")
	FLinearColor TintColor = FLinearColor::White;

	/**
	 *  Surface opacity (0 = fully transparent, 1 = fully opaque).
	 *  Requires the material to use the Opacity parameter in a Translucent blend.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 1.f;

	/**
	 *  Uniform scale applied to the icon quad.
	 *  The engine plane is 100 × 100 cm at scale 1.0, so:
	 *    0.3 → 30 cm,  0.5 → 50 cm,  1.0 → 100 cm (1 m).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Appearance",
		meta = (ClampMin = "0.001"))
	float WorldScale = 0.5f;

	/**
	 *  Offset of the icon quad relative to this actor's root.
	 *  Positive Z lifts the icon above whatever actor this is attached to.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Appearance")
	FVector RelativeOffset = FVector(0.f, 0.f, 50.f);

	// ── Behaviour ─────────────────────────────────────────────────────────────

	/**
	 *  When true (default), the icon faces the player camera every Tick.
	 *  When false, the mesh keeps its spawn-time world rotation unchanged.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Behaviour")
	bool bEnableBillboard = true;

	/**
	 *  When true, only the Yaw axis is driven toward the camera.
	 *  The icon stays perfectly vertical — useful for upright signs or prompt icons.
	 *  When false, the full 3-axis rotation is applied so the icon faces the camera
	 *  even when viewed from directly above or below.
	 *  Only relevant when bEnableBillboard is true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Behaviour",
		meta = (EditCondition = "bEnableBillboard", EditConditionHides))
	bool bYawOnly = true;

	/**
	 *  When true and bIsActive is false, the icon mesh is hidden (SetVisibility).
	 *  When false, visibility is never touched regardless of bIsActive — you can
	 *  instead drive Opacity to 0 for a fade-out effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Behaviour")
	bool bHiddenWhenDisabled = true;

	// ── State ─────────────────────────────────────────────────────────────────

	/**
	 *  Whether the icon is currently shown.
	 *  Call SetActive(false) to hide a consumed switch's icon.
	 *  Visibility is applied only when bHiddenWhenDisabled is true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|State")
	bool bIsActive = true;

	// ── Material parameter names ───────────────────────────────────────────────

	/**
	 *  Name of the Texture2D parameter in IconMaterial that receives IconTexture.
	 *  Change this if your material uses a different parameter name.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Material")
	FName TextureParameterName = TEXT("IconTexture");

	/** Name of the Vector parameter that receives TintColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Material")
	FName ColorParameterName = TEXT("TintColor");

	/** Name of the Scalar parameter that receives Opacity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon|Material")
	FName OpacityParameterName = TEXT("Opacity");

	// ── Blueprint-callable API ────────────────────────────────────────────────

	/** Activates or deactivates the icon. Updates mesh visibility when bHiddenWhenDisabled is true. */
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetActive(bool bActive);

	/**
	 *  Assigns a new texture and immediately pushes it to the dynamic material.
	 *  Call this at runtime to swap icon sprites (e.g. key → open padlock).
	 */
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetIconTexture(UTexture2D* NewTexture);

	/** Updates TintColor and pushes the change to the dynamic material. */
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetTintColor(FLinearColor NewColor);

	/** Updates Opacity and pushes the change to the dynamic material. */
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetOpacity(float NewOpacity);

	/**
	 *  Resizes the icon quad.  Same scale convention as the WorldScale property.
	 *  Example: SetWorldScale(0.3f) makes the icon 30 × 30 cm.
	 */
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetWorldScale(float NewScale);

	/**
	 *  Repositions the icon relative to this actor's root.
	 *  Example: SetRelativeOffset(FVector(0, 0, 80)) floats the icon 80 cm above.
	 */
	UFUNCTION(BlueprintCallable, Category = "Icon")
	void SetRelativeOffset(FVector NewOffset);

private:

	/**
	 *  Dynamic material instance wrapping IconMaterial.
	 *  Created (or reused) by InitDynMaterial.  All parameter writes go here
	 *  so the shared IconMaterial asset is never modified at runtime.
	 */
	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial = nullptr;

	/**
	 *  Ensures DynMaterial is a valid DMI wrapping the current IconMaterial.
	 *
	 *  If slot 0 of IconMesh is already a DMI whose Parent matches IconMaterial,
	 *  it is reused (no DMI-of-DMI chaining across multiple OnConstruction calls).
	 *  If IconMaterial has changed since the last call, a fresh DMI is created.
	 *  No-op when IconMesh has no material or IconMaterial is null.
	 */
	void InitDynMaterial();

	/**
	 *  Pushes IconTexture, TintColor, and Opacity to DynMaterial.
	 *  Parameters that do not exist in the material are silently ignored.
	 *  No-op when DynMaterial is null.
	 */
	void ApplyMaterialParameters() const;

	/**
	 *  Shows or hides IconMesh according to bIsActive and bHiddenWhenDisabled.
	 *  Called from BeginPlay, OnConstruction, and SetActive.
	 */
	void UpdateVisibility() const;

	/**
	 *  Applies RelativeOffset and WorldScale to IconMesh.
	 *  Called from BeginPlay and OnConstruction so the editor viewport reflects
	 *  property changes live.
	 */
	void ApplyTransform() const;

	/**
	 *  Rotates IconMesh each frame so its +Z normal faces the player camera.
	 *  Respects bEnableBillboard and bYawOnly.
	 *  No-op when bEnableBillboard is false or no camera manager is available.
	 */
	void UpdateBillboard() const;
};

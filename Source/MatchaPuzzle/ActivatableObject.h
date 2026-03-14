// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActivatableTarget.h"
#include "ActivatableObject.generated.h"

class UStaticMeshComponent;

/**
 *  A scene object that appears (Activate) and disappears (Deactivate) when
 *  triggered by a lever's ActivatableTargets list.
 *
 *  Activate   → visible   + collision enabled
 *  Deactivate → invisible + collision disabled
 *
 *  Setup
 *  ─────
 *   1. Create a Blueprint child of AActivatableObject (e.g. BP_SecretBlock).
 *   2. In the Blueprint Details panel, assign a Static Mesh to the Mesh component.
 *   3. Place the Blueprint instance in the level.
 *   4. Assign the placed instance to the lever's "Activatable Targets" array.
 *   5. bStartHidden = true (default) means the object is invisible at level start.
 *      Set it to false if you want the object visible by default and hidden on activate
 *      — swap your Activate/Deactivate logic in that case.
 */
UCLASS()
class AActivatableObject : public AActor, public IActivatableTarget
{
	GENERATED_BODY()

	/** The visible mesh. Assign any Static Mesh asset in the Details panel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;

public:

	AActivatableObject();

	virtual void BeginPlay() override;

	/** Show the object and re-enable its collision. */
	virtual void Activate_Implementation() override;

	/** Hide the object and disable its collision. */
	virtual void Deactivate_Implementation() override;

protected:

	/**
	 * When true (default), the object starts invisible and non-collidable at
	 * level load, waiting for the lever to activate it.
	 * Set to false if you want it visible at start instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activatable Object")
	bool bStartHidden = true;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PuzzleCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UNiagaraSystem;

/**
 *  A controllable top-down perspective character
 */
UCLASS(abstract)
class APuzzleCharacter : public ACharacter
{
	GENERATED_BODY()

private:

	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

public:

	/** Constructor */
	APuzzleCharacter();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Returns the camera component **/
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent.Get(); }

	/** Returns the Camera Boom component **/
	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

	/**
	 * Spawns the click indicator effect at the given world location.
	 * Called by the PlayerController on each confirmed left-click hit.
	 * No-ops silently when FXCursor is not assigned.
	 */
	UFUNCTION(BlueprintCallable, Category="Click Indicator")
	void SpawnClickIndicator(const FVector& Location);

protected:

	// --- Click Indicator ---

	/** Niagara effect spawned at the clicked world location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Click Indicator")
	TObjectPtr<UNiagaraSystem> FXCursor;

	/** Uniform scale applied to the click indicator effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Click Indicator", meta=(ClampMin="0.01"))
	float CursorEffectScale = 1.f;


};


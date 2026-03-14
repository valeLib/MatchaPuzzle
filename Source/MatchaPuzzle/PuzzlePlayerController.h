// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "PuzzlePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class USpringArmComponent;

/**
 *  Player controller for a top-down perspective game.
 *  Implements WASD directional movement and camera orbit/pan controls.
 */
UCLASS(abstract)
class APuzzlePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** WASD movement action (Axis2D) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Camera Orbit Rotation Input Action (Axis2D - mouse delta, chorded with right click) */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> RotateCameraAction;

	/** Camera Pan Input Action (Axis2D - mouse delta, chorded with middle mouse) */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> PanCameraAction;

	/** Right Click helper action (used as chord for orbit) */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> RightClickAction;

	/** Middle Click helper action (used as chord for pan) */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MiddleClickAction;

	// Zoom input
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> ZoomAction;

	/** Camera orbit rotation speed multiplier */
	UPROPERTY(EditAnywhere, Category="Camera")
	float CameraRotationSpeed = 1.0f;

	/** Camera pan speed multiplier */
	UPROPERTY(EditAnywhere, Category="Camera")
	float CameraPanSpeed = 2.0f;

	UPROPERTY(EditAnywhere, Category="Camera|Pan")
	float MaxPanX = 150.f;

	UPROPERTY(EditAnywhere, Category="Camera|Pan")
	float MaxPanY = 150.f;

	UPROPERTY(EditAnywhere, Category="Camera|Pan")
	float PanZ = 0.f; // keep 0

	/** Minimum camera pitch angle (looking up limit) */
	UPROPERTY(EditAnywhere, Category="Camera")
	float MinCameraPitch = -80.0f;

	/** Maximum camera pitch angle (looking down limit) */
	UPROPERTY(EditAnywhere, Category="Camera")
	float MaxCameraPitch = -10.0f;

	UPROPERTY(BlueprintReadWrite, Category="UI")
	bool bUIOpen = true;

	public:
		UFUNCTION(BlueprintCallable, Category="UI")
		void SetUIOpen(bool bOpen);

public:

	/** Constructor */
	APuzzlePlayerController();

protected:

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;

	/** WASD directional movement handler */
	void OnMove(const FInputActionValue& Value);

	/** Camera orbit rotation handler (right click + drag) */
	void OnRotateCamera(const FInputActionValue& Value);

	/** Camera pan handler (middle mouse + drag) */
	void OnPanCamera(const FInputActionValue& Value);

	void DebugRightDown();
	void DebugRightUp();
};

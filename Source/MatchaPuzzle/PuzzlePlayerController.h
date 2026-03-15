// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "PuzzlePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class AClawMachineLever;

/**
 *  Player controller for a top-down perspective game.
 *  Implements WASD directional movement and camera orbit/pan controls.
 *
 *  Lever interaction model:
 *   - When the player enters a lever's trigger, the lever calls
 *     SetCurrentNearbyLever(this).  The controller remembers it.
 *   - Pressing E (InteractAction) begins lever control:
 *       ActiveControlledLever is set, WASD is routed to the lever.
 *   - Pressing E again ends lever control:
 *       ActiveControlledLever is cleared, WASD resumes character movement.
 *   - When the lever exits the trigger (player walks away), it calls
 *     EndControl() internally, which calls OnLeverControlEnded so the
 *     controller can clear its own reference.
 */
UCLASS(abstract)
class APuzzlePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** WASD movement action (Axis2D).  Also drives lever when in lever control mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	/**
	 * Interaction action (E key).
	 * Press near a lever to begin controlling it.
	 * Press again to release control and resume normal movement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> InteractAction;

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

	// ── Lever interaction API ─────────────────────────────────────────────────

	/**
	 * Called by AClawMachineLever::OnTriggerBeginOverlap when the player enters
	 * a lever's interaction sphere.  The lever is remembered as the candidate
	 * for the next E press.
	 */
	void SetCurrentNearbyLever(AClawMachineLever* Lever);

	/**
	 * Called by AClawMachineLever::OnTriggerEndOverlap when the player leaves.
	 * Only clears CurrentNearbyLever if it matches the departing lever, so two
	 * overlapping levers cannot accidentally clear each other's state.
	 */
	void ClearCurrentNearbyLever(AClawMachineLever* Lever);

	/**
	 * Called by AClawMachineLever::EndControl when the lever releases itself
	 * (e.g. player walks away mid-control).  Clears ActiveControlledLever so
	 * WASD returns to character movement without requiring another E press.
	 */
	void OnLeverControlEnded(AClawMachineLever* Lever);

protected:

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;

	/**
	 * WASD directional movement handler.
	 * When ActiveControlledLever is set, routes input to the lever instead
	 * of the character so normal movement is fully blocked during lever control.
	 */
	void OnMove(const FInputActionValue& Value);

	/**
	 * Interact handler (E key — ETriggerEvent::Started, fires once per press).
	 * Toggles lever control on/off when a lever is nearby or active.
	 */
	void OnInteract(const FInputActionValue& Value);

	/** Camera orbit rotation handler (right click + drag) */
	void OnRotateCamera(const FInputActionValue& Value);

	/** Camera pan handler (middle mouse + drag) */
	void OnPanCamera(const FInputActionValue& Value);

	void DebugRightDown();
	void DebugRightUp();

private:

	/**
	 * The lever the player is currently near (inside its interaction sphere).
	 * Set by SetCurrentNearbyLever, cleared by ClearCurrentNearbyLever.
	 * Used to identify which lever to begin controlling when E is pressed.
	 */
	TWeakObjectPtr<AClawMachineLever> CurrentNearbyLever;

	/**
	 * The lever the player is actively steering with WASD.
	 * Non-null while in lever control mode.  WASD input is routed here instead
	 * of to the character for as long as this reference is valid.
	 * Cleared on a second E press or when the lever calls OnLeverControlEnded.
	 */
	TWeakObjectPtr<AClawMachineLever> ActiveControlledLever;
};

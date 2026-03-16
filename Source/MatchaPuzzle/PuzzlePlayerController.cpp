// Copyright Epic Games, Inc. All Rights Reserved.

#include "PuzzlePlayerController.h"
#include "ClawMachineLever.h"
#include "GameFramework/Pawn.h"
#include "PuzzleCharacter.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/SpringArmComponent.h"
#include "MatchaPuzzle.h"

APuzzlePlayerController::APuzzlePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void APuzzlePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	// Add Input Mapping Context
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// WASD movement (also routes to lever when in lever control mode)
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APuzzlePlayerController::OnMove);

		// E key — begin / end lever control
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &APuzzlePlayerController::OnInteract);

		// Camera orbit (right click + drag)
		EIC->BindAction(RotateCameraAction, ETriggerEvent::Triggered, this, &APuzzlePlayerController::OnRotateCamera);

		// Camera pan (middle mouse + drag)
		EIC->BindAction(PanCameraAction, ETriggerEvent::Triggered, this, &APuzzlePlayerController::OnPanCamera);

		// Right click chord helper
		EIC->BindAction(RightClickAction, ETriggerEvent::Started,    this, &APuzzlePlayerController::DebugRightDown);
		EIC->BindAction(RightClickAction, ETriggerEvent::Completed,  this, &APuzzlePlayerController::DebugRightUp);
		EIC->BindAction(RightClickAction, ETriggerEvent::Canceled,   this, &APuzzlePlayerController::DebugRightUp);
	}
	else
	{
		UE_LOG(LogMatchaPuzzle, Error, TEXT("'%s' Failed to find an Enhanced Input Component!"), *GetNameSafe(this));
	}
}

void APuzzlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->ClearAllMappings();
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	SetUIOpen(true);
}

// ── Lever interaction API ──────────────────────────────────────────────────────

void APuzzlePlayerController::SetCurrentNearbyLever(AClawMachineLever* Lever)
{
	CurrentNearbyLever = Lever;
}

void APuzzlePlayerController::ClearCurrentNearbyLever(AClawMachineLever* Lever)
{
	// Only clear if this is the lever we're tracking — prevents a second lever's
	// exit overlap from accidentally erasing a different lever's entry.
	if (CurrentNearbyLever.Get() == Lever)
	{
		CurrentNearbyLever.Reset();
	}
}

void APuzzlePlayerController::OnLeverControlEnded(AClawMachineLever* Lever)
{
	// Called by the lever when it releases itself (e.g. player walks away).
	// Clear the active reference so WASD returns to character movement.
	if (ActiveControlledLever.Get() == Lever)
	{
		ActiveControlledLever.Reset();
	}
}

// ── Input handlers ─────────────────────────────────────────────────────────────

void APuzzlePlayerController::OnInteract(const FInputActionValue& /*Value*/)
{
	// If already controlling a lever, end that session.
	if (AClawMachineLever* ActiveLever = ActiveControlledLever.Get())
	{
		ActiveLever->EndControl();
		// ActiveControlledLever is cleared by OnLeverControlEnded callback above.
		return;
	}

	// If a lever is nearby, begin controlling it.
	if (AClawMachineLever* NearbyLever = CurrentNearbyLever.Get())
	{
		NearbyLever->BeginControl(this);
		ActiveControlledLever = NearbyLever;
	}
}

void APuzzlePlayerController::OnMove(const FInputActionValue& Value)
{
	const FVector2D MoveVector = Value.Get<FVector2D>();

	// ── Lever control mode ────────────────────────────────────────────────────
	// When a lever is active, WASD drives the lever exclusively.
	// Normal character movement is blocked for the duration.
	//
	// Input mapping:
	//   MoveVector.X  (+D / −A) → horizontal lever value
	//   MoveVector.Y  (+W / −S) → vertical lever value
	//     W (positive) → restore toward neutral (clamped at VerticalMax = 0)
	//     S (negative) → push lever downward

	if (AClawMachineLever* ActiveLever = ActiveControlledLever.Get())
	{
		const float DT = GetWorld()->GetDeltaSeconds();
		ActiveLever->AddHorizontalInput(MoveVector.X, DT);
		ActiveLever->AddVerticalInput(-MoveVector.Y, DT);
		return;
	}

	// ── Normal character movement ─────────────────────────────────────────────

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	APuzzleCharacter* LevelCharacter = Cast<APuzzleCharacter>(ControlledPawn);
	if (!LevelCharacter)
	{
		return;
	}

	USpringArmComponent* Boom = LevelCharacter->GetCameraBoom();
	if (!Boom)
	{
		return;
	}

	// CameraBoom uses SetUsingAbsoluteRotation(true), so GetRelativeRotation()
	// gives the world-space yaw of the camera — movement is always camera-relative.
	const float CameraYaw = Boom->GetRelativeRotation().Yaw;
	const FRotator YawRotation(0.f, CameraYaw, 0.f);

	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// MoveVector.Y = forward/back (W/S), MoveVector.X = strafe (D/A)
	ControlledPawn->AddMovementInput(ForwardDir, MoveVector.Y);
	ControlledPawn->AddMovementInput(RightDir,   MoveVector.X);
}

void APuzzlePlayerController::OnRotateCamera(const FInputActionValue& Value)
{
	if (bUIOpen)
	{
		return;
	}
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	UE_LOG(LogMatchaPuzzle, Log, TEXT("OnRotateCamera: X=%.2f Y=%.2f"), LookAxisVector.X, LookAxisVector.Y);

	APuzzleCharacter* ControlledCharacter = Cast<APuzzleCharacter>(GetPawn());
	if (!ControlledCharacter)
	{
		UE_LOG(LogMatchaPuzzle, Error, TEXT("Pawn is not APuzzleCharacter. Pawn=%s"), *GetNameSafe(GetPawn()));
		return;
	}

	USpringArmComponent* CameraBoom = ControlledCharacter->GetCameraBoom();
	if (!CameraBoom)
	{
		return;
	}

	FRotator CurrentRotation = CameraBoom->GetRelativeRotation();

	float NewYaw   = CurrentRotation.Yaw + LookAxisVector.X * CameraRotationSpeed;
	float NewPitch = FMath::Clamp(
		CurrentRotation.Pitch - LookAxisVector.Y * CameraRotationSpeed,
		MinCameraPitch,
		MaxCameraPitch
	);

	CameraBoom->SetRelativeRotation(FRotator(NewPitch, NewYaw, 0.0f));
}

void APuzzlePlayerController::OnPanCamera(const FInputActionValue& Value)
{
	const FVector2D PanVector = Value.Get<FVector2D>();

	APuzzleCharacter* ControlledCharacter = Cast<APuzzleCharacter>(GetPawn());
	if (!ControlledCharacter)
	{
		return;
	}

	USpringArmComponent* CameraBoom = ControlledCharacter->GetCameraBoom();
	if (!CameraBoom)
	{
		return;
	}

	FRotator BoomRotation = CameraBoom->GetRelativeRotation();
	FRotator YawRotation(0.0f, BoomRotation.Yaw, 0.0f);

	FVector RightDirection   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	FVector PanDelta = (RightDirection * -PanVector.X + ForwardDirection * PanVector.Y) * CameraPanSpeed;
	CameraBoom->TargetOffset += PanDelta;

	CameraBoom->TargetOffset.X = FMath::Clamp(CameraBoom->TargetOffset.X, -MaxPanX, MaxPanX);
	CameraBoom->TargetOffset.Y = FMath::Clamp(CameraBoom->TargetOffset.Y, -MaxPanY, MaxPanY);
	CameraBoom->TargetOffset.Z = PanZ;
}

void APuzzlePlayerController::DebugRightDown()
{
	UE_LOG(LogMatchaPuzzle, Warning, TEXT("RMB down"));
}

void APuzzlePlayerController::DebugRightUp()
{
	UE_LOG(LogMatchaPuzzle, Warning, TEXT("RMB up"));
}

void APuzzlePlayerController::SetUIOpen(bool bOpen)
{
	bUIOpen = bOpen;

	if (bUIOpen)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;

		FInputModeGameOnly Mode;
		SetInputMode(Mode);
	}
}

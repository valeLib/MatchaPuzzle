// Copyright Epic Games, Inc. All Rights Reserved.

#include "PuzzleCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"

APuzzleCharacter::APuzzleCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = false;
	GetCharacterMovement()->bSnapToPlaneAtStart = false;

	// Allow walking up steeper ramps
	GetCharacterMovement()->SetWalkableFloorAngle(50.f);

	// Do not inherit the velocity of a moving platform when the character leaves it.
	// Without these flags, walking off an upward-moving platform imparts the
	// platform's velocity to the character and launches them into the air.
	GetCharacterMovement()->bImpartBaseVelocityX = false;
	GetCharacterMovement()->bImpartBaseVelocityY = false;
	GetCharacterMovement()->bImpartBaseVelocityZ = false;

	// Create the camera boom component
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));

	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetUsingAbsoluteRotation(true); // keep camera rotation world-space so it doesn't spin with the character
	CameraBoom->TargetArmLength = 2000.f;
	CameraBoom->SetRelativeRotation(FRotator(-55.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	// Create the camera component
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));

	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void APuzzleCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APuzzleCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void APuzzleCharacter::SpawnClickIndicator(const FVector& Location)
{
	if (!FXCursor)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		FXCursor,
		Location,
		FRotator::ZeroRotator,
		FVector(CursorEffectScale),
		/*bAutoDestroy=*/ true,
		/*bAutoActivate=*/ true,
		ENCPoolMethod::None,
		/*bPreCullCheck=*/ true);
}

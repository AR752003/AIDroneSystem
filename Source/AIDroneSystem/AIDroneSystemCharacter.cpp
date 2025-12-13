// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIDroneSystemCharacter.h"

#include "AIDrone.h"
#include "DroneControllerComponent.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AAIDroneSystemCharacter

AAIDroneSystemCharacter::AAIDroneSystemCharacter()
{

	TraceLength = 600.f;
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AAIDroneSystemCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAIDroneSystemCharacter, AIDrone);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AAIDroneSystemCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AAIDroneSystemCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAIDroneSystemCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAIDroneSystemCharacter::Look);

		EnhancedInputComponent->BindAction(FollowDrone, ETriggerEvent::Started, this, &AAIDroneSystemCharacter::DroneFollowMe);
		EnhancedInputComponent->BindAction(UnFollowDrone, ETriggerEvent::Started, this, &AAIDroneSystemCharacter::DroneUnfollowMe);
		EnhancedInputComponent->BindAction(PossessDrone, ETriggerEvent::Started, this, &AAIDroneSystemCharacter::PossessDroneRequest);
		EnhancedInputComponent->BindAction(DroneInteraction, ETriggerEvent::Started, this, &AAIDroneSystemCharacter::InteractDroneRequest);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AAIDroneSystemCharacter::DroneFollowMe()
{
	UE_LOG(LogTemp, Display, TEXT("DroneFollowMe Begin"));
	// === MODIFIED: Null Check to prevent crash ===
	if (AIDrone && AIDrone->IsValidLowLevel())
	{
		AIDrone->ServerFollowMe(this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneFollowMe failed: AIDrone is NULL!"));
	}
}

void AAIDroneSystemCharacter::DroneUnfollowMe()
{
	UE_LOG(LogTemp, Display, TEXT("DroneUnFollowMe Begin"));
	// === MODIFIED: Null Check to prevent crash ===
	if (AIDrone && AIDrone->IsValidLowLevel())
	{
		AIDrone->ServerUnfollow();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneUnfollowMe failed: AIDrone is NULL!"));
	}
}

void AAIDroneSystemCharacter::PossessDroneRequest()
{
	UE_LOG(LogTemp, Display, TEXT("Possess Drone Request"));
	
	if (AIDrone && AIDrone->IsValidLowLevel())
	{
		APlayerController* PlayerController = GetController<APlayerController>();

		// === CRITICAL FIX: Call the RPC on the OWNED Character (this) ===
		// The client calls ServerRequestPossessDrone, which is guaranteed to be forwarded
		// to the server because 'this' (AAIDroneSystemCharacter) is owned by the client's controller.
		if (PlayerController && IsLocallyControlled())
		{
			// Note: Use 'this' Character's controller as the Requester
			ServerRequestPossessDrone(AIDrone, PlayerController);
		}
		// ===============================================================
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PossessDroneRequest failed: AIDrone is NULL!"));
	}
}

void AAIDroneSystemCharacter::InteractDroneRequest()
{
	FVector2D ViewportSize;
	GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
	FVector2D ViewportCenter = ViewportSize/2;
	FVector TraceStart;
	FVector Forward;
	APlayerController* PlayerController = GetController<APlayerController>();
	if (!UGameplayStatics::DeprojectScreenToWorld(PlayerController, ViewportCenter, TraceStart, Forward)) return;

	const FVector TraceEnd = TraceStart + Forward * TraceLength;

	FHitResult HitResult;

	GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, DroneInteractionChannel);

	AActor* HitActor = HitResult.GetActor();
	AAIDrone* DroneActor =  Cast<AAIDrone>(HitActor);
	if (HitActor && DroneActor)
	{
		AIDrone = DroneActor;
		// Optionally, log this assignment for debugging.
		UE_LOG(LogTemp, Display, TEXT("AIDrone successfully assigned via interaction!"));
	}
}

void AAIDroneSystemCharacter::ServerRequestPossessDrone_Implementation(AAIDrone* DroneToPossess,
	APlayerController* Requester)
{
	UE_LOG(LogTemp, Display, TEXT("Server: Processing Possess Drone Request."));
    
	// Server checks the required distance and state (using AIDrone's original logic)
	if (DroneToPossess && Requester && DroneToPossess->CurrentState != EDroneState::Possessed)
	{
		if (APawn* PlayerPawn = Requester->GetPawn())
		{
			if (FVector::Dist(DroneToPossess->GetActorLocation(), PlayerPawn->GetActorLocation()) <= DroneToPossess->CommandRange)
			{
				// Unpossess the character, and possess the drone
				// Note: UnPossess is not necessary if the Controller automatically handles it, 
				// but it's safe to be explicit here before possession.
				Requester->Possess(DroneToPossess); 
                
				UE_LOG(LogTemp, Display, TEXT("Server: Successfully possessed drone: %s"), *DroneToPossess->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Server: Drone too far to possess."));
			}
		}
	}
}

bool AAIDroneSystemCharacter::ServerRequestPossessDrone_Validate(AAIDrone* DroneToPossess, APlayerController* Requester)
{
	return (Requester != nullptr && DroneToPossess != nullptr);
}

void AAIDroneSystemCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AAIDroneSystemCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}


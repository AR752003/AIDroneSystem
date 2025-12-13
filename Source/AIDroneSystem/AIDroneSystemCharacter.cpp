// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIDroneSystemCharacter.h"
#include "AIDrone.h"
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
}

void AAIDroneSystemCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAIDroneSystemCharacter, AIDrone);
}

//////////////////////////////////////////////////////////////////////////
// Possession Handling

void AAIDroneSystemCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void AAIDroneSystemCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	// This runs on the client when PlayerState replicates
	// Add input mapping context here for the client
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (PlayerController->IsLocalController())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->ClearAllMappings();
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AAIDroneSystemCharacter::UnPossessed()
{
	Super::UnPossessed();
	
	// Remove input mapping context when character is unpossessed
	if (APlayerController* PlayerController = GetController<APlayerController>())
	{
		if (PlayerController->IsLocalController())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(DefaultMappingContext);
			}
		}
	}
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

// === Client-side wrappers to initiate Server RPC ===
void AAIDroneSystemCharacter::DroneFollowMe()
{
	if (AIDrone && AIDrone->IsValidLowLevel() && IsLocallyControlled())
	{
		ServerRequestDroneFollow(AIDrone, this);
	}
}

void AAIDroneSystemCharacter::DroneUnfollowMe()
{
	if (AIDrone && AIDrone->IsValidLowLevel() && IsLocallyControlled())
	{
		ServerRequestDroneUnfollow(AIDrone);
	}
}
// ====================================================

void AAIDroneSystemCharacter::PossessDroneRequest()
{
	if (AIDrone && AIDrone->IsValidLowLevel())
	{
		APlayerController* PlayerController = GetController<APlayerController>();

		if (PlayerController && IsLocallyControlled())
		{
			ServerRequestPossessDrone(AIDrone, PlayerController);
		}
	}
}

void AAIDroneSystemCharacter::InteractDroneRequest()
{
    // New logic: Only trace for a new drone if the player doesn't already have a valid reference.
    if (AIDrone)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot interact: Player already controls a drone."));
        return;
    }
    
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
	
    // Set the reference only if a new valid drone is found.
	if (DroneActor)
	{
		AIDrone = DroneActor;
        UE_LOG(LogTemp, Warning, TEXT("Drone reference acquired: %s"), *AIDrone->GetName());
	}
}

// === Server RPC Implementations (Running on Server) ===
bool AAIDroneSystemCharacter::ServerRequestDroneFollow_Validate(AAIDrone* DroneToCommand, ACharacter* Player)
{
	return (DroneToCommand != nullptr && Player != nullptr);
}

void AAIDroneSystemCharacter::ServerRequestDroneFollow_Implementation(AAIDrone* DroneToCommand, ACharacter* Player)
{
	if (DroneToCommand && Player)
	{
		if (FVector::Dist(DroneToCommand->GetActorLocation(), Player->GetActorLocation()) <= DroneToCommand->CommandRange)
		{
			DroneToCommand->FollowTarget = Player;
			DroneToCommand->CurrentState = EDroneState::Following;
			
			// Use public wrapper to update visuals on the server
			DroneToCommand->UpdateVisualsAfterStateChange(); 
		}
	}
}

bool AAIDroneSystemCharacter::ServerRequestDroneUnfollow_Validate(AAIDrone* DroneToCommand)
{
	return (DroneToCommand != nullptr);
}

void AAIDroneSystemCharacter::ServerRequestDroneUnfollow_Implementation(AAIDrone* DroneToCommand)
{
	if (DroneToCommand)
	{
		DroneToCommand->FollowTarget = nullptr;
		DroneToCommand->CurrentState = EDroneState::Idle;
		
		// Use public wrapper to update visuals on the server
		DroneToCommand->UpdateVisualsAfterStateChange(); 
	}
}

void AAIDroneSystemCharacter::ServerRequestPossessDrone_Implementation(AAIDrone* DroneToPossess,
                                                                       APlayerController* Requester)
{
	if (DroneToPossess && Requester && DroneToPossess->CurrentState != EDroneState::Possessed)
	{
		if (APawn* PlayerPawn = Requester->GetPawn())
		{
			if (FVector::Dist(DroneToPossess->GetActorLocation(), PlayerPawn->GetActorLocation()) <= DroneToPossess->CommandRange)
			{
				Requester->Possess(DroneToPossess); 
			}
		}
	}
}

bool AAIDroneSystemCharacter::ServerRequestPossessDrone_Validate(AAIDrone* DroneToPossess, APlayerController* Requester)
{
	return (Requester != nullptr && DroneToPossess != nullptr);
}
// ====================================================

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
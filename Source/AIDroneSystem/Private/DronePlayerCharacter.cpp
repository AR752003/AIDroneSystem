#include "DronePlayerCharacter.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIDrone.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ADronePlayerCharacter::ADronePlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
    GetCharacterMovement()->JumpZVelocity = 700.f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
}

void ADronePlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(PlayerMappingContext, 0);
        }
    }
}

void ADronePlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADronePlayerCharacter::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADronePlayerCharacter::Look);
        EnhancedInputComponent->BindAction(FollowDroneAction, ETriggerEvent::Started, this, &ADronePlayerCharacter::FollowDrone);
        EnhancedInputComponent->BindAction(UnfollowDroneAction, ETriggerEvent::Started, this, &ADronePlayerCharacter::UnfollowDrone);
        EnhancedInputComponent->BindAction(PossessDroneAction, ETriggerEvent::Started, this, &ADronePlayerCharacter::PossessDrone);
    }
}

void ADronePlayerCharacter::Move(const FInputActionValue& Value)
{
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        AddMovementInput(GetActorRightVector(), MovementVector.X);
        AddMovementInput(GetActorForwardVector(), MovementVector.Y);
    }
}

void ADronePlayerCharacter::Look(const FInputActionValue& Value)
{
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void ADronePlayerCharacter::FollowDrone()
{
    UE_LOG(LogTemp, Display, TEXT("Follow Drone Begin"));
    
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        FVector ViewLocation;
        FRotator ViewRotation;  // Correct type

        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

        FVector Start = ViewLocation;
        FVector End = Start + (ViewRotation.Vector() * InteractRange);

        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
        {
            if (AAIDrone* Drone = Cast<AAIDrone>(Hit.GetActor()))
            {
                Drone->ServerFollowMe(this);
            }
        }
    }
}

void ADronePlayerCharacter::UnfollowDrone()
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        FVector ViewLocation;
        FRotator ViewRotation;

        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

        FVector Start = ViewLocation;
        FVector End = Start + (ViewRotation.Vector() * InteractRange);

        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
        {
            if (AAIDrone* Drone = Cast<AAIDrone>(Hit.GetActor()))
            {
                Drone->ServerUnfollow();
            }
        }
    }
}

void ADronePlayerCharacter::PossessDrone()
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        FVector ViewLocation;
        FRotator ViewRotation;

        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

        FVector Start = ViewLocation;
        FVector End = Start + (ViewRotation.Vector() * InteractRange);

        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
        {
            if (AAIDrone* Drone = Cast<AAIDrone>(Hit.GetActor()))
            {
                Drone->ServerRequestPossess(PC);
            }
        }
    }
}
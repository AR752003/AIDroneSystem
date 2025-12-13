#include "AIDrone.h"
#include "AIDronePlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

AAIDrone::AAIDrone()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicatingMovement(true);

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    DroneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneMesh"));
    DroneMesh->SetupAttachment(Root);
    DroneMesh->SetIsReplicated(true);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Root);
    Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));

    MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
    MovementComponent->UpdatedComponent = Root;
    MovementComponent->MaxSpeed = 800.0f;
    MovementComponent->Acceleration = 2048.0f;
    MovementComponent->Deceleration = 2048.0f;

    CurrentState = EDroneState::Idle;

    bUseControllerRotationPitch = true;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = true;
}

void AAIDrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAIDrone, CurrentState);
    DOREPLIFETIME(AAIDrone, FollowTarget);
}

void AAIDrone::BeginPlay()
{
    Super::BeginPlay();
    UpdateVisualFeedback();
}

void AAIDrone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // On the controlling client, send movement to server
    if (IsLocallyControlled() && !HasAuthority())
    {
        FVector InputVector = ConsumeMovementInputVector();
        
        if (!InputVector.IsNearlyZero() || GetVelocity().SizeSquared() > 1.0f)
        {
            // Send movement data to server
            ServerMove(GetActorLocation(), InputVector, GetControlRotation(), DeltaTime);
        }
    }

    // On server (authority), process movement for all pawns
   if (HasAuthority())
    {
        // Only run autonomous (AI) behavior if the drone is not player-controlled.
        if (CurrentState == EDroneState::Possessed)
        {
            // Player-controlled, movement is handled by AddMovementInput calls from input (RPC)
        }
        else // Autonomous behavior when Idle or Following
        {
            if (CurrentState == EDroneState::Following && FollowTarget && FollowTarget->IsValidLowLevel())
            {
                FVector Dir = FollowTarget->GetActorLocation() - GetActorLocation();
                float Dist = Dir.Size();
                
                // === Obstacle Avoidance Logic (Simple Ray-Casting) ===
                FVector AvoidanceVector = FVector::ZeroVector;
                float AvoidanceDistance = 300.0f; // Distance to check for obstacles
                float AvoidanceForce = 1.0f;
                FHitResult HitResult;
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(this);
                
                // Trace forward to check for immediate obstacles
                if (GetWorld()->LineTraceSingleByChannel(HitResult, GetActorLocation(), GetActorLocation() + Dir.GetSafeNormal() * AvoidanceDistance, ECC_Visibility, Params))
                {
                    FVector HitNormal = HitResult.Normal;
                    
                    // Prioritize moving up, if possible.
                    if (FMath::Abs(HitNormal.Z) < 0.7f)
                    {
                        AvoidanceVector = FVector::CrossProduct(HitNormal, GetActorRightVector());
                        AvoidanceVector.Z += 0.5f; 
                    }
                    else
                    {
                        // Mostly hitting floor or ceiling, try to move sideways
                        AvoidanceVector = GetActorRightVector();
                    }
                    AvoidanceVector = AvoidanceVector.GetSafeNormal() * AvoidanceForce;
                }
                // ======================================================

                FVector TargetDirection = Dir.GetSafeNormal();
                FVector FinalMoveDirection = (TargetDirection + AvoidanceVector).GetSafeNormal();

                // 1. ROTATION: Rotate towards the final movement direction
                FRotator TargetRot = FinalMoveDirection.Rotation();
                TargetRot.Pitch = 0.0f;
                TargetRot.Roll = 0.0f;
                FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 8.0f);
                SetActorRotation(NewRot);
                
                // 2. MOVEMENT: Apply continuous movement input.
                // The amount of input should be proportional to the distance to maintain the follow distance smoothly.
                // Use the component's MaxSpeed to scale the input correctly.
                
                float MovementMagnitude = 0.0f;

                if (Dist > FollowDistance)
                {
                    // Scale movement by how far outside the follow distance we are (clamped)
                    MovementMagnitude = FMath::Clamp((Dist - FollowDistance) / FollowDistance, 0.1f, 1.0f);
                }
                
                // Apply the calculated movement input
                AddMovementInput(FinalMoveDirection, MovementMagnitude);
                
            }
            else if (CurrentState == EDroneState::Idle)
            {
                // Corrected Idle logic: Only hover movement, NO forward drift.
                float Time = GetWorld()->GetTimeSeconds();
                float HoverOffset = FMath::Sin(Time * HoverFrequency) * HoverAmplitude;
                float HoverVelZ = (HoverOffset - LastHoverOffset) / DeltaTime;
                AddMovementInput(FVector::UpVector, HoverVelZ / MovementComponent->MaxSpeed);
                LastHoverOffset = HoverOffset;
            }
        }
    }
}

void AAIDrone::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(DroneForwardAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveForward);
        EnhancedInputComponent->BindAction(DroneRightAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveRight);
        EnhancedInputComponent->BindAction(DroneUpAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveUp);
        EnhancedInputComponent->BindAction(DroneYawAction, ETriggerEvent::Triggered, this, &AAIDrone::Turn);
        EnhancedInputComponent->BindAction(DronePitchAction, ETriggerEvent::Triggered, this, &AAIDrone::LookUp);
        EnhancedInputComponent->BindAction(UnpossessAction, ETriggerEvent::Triggered, this, &AAIDrone::Unpossess);
    }
}

// === PUBLIC FUNCTION IMPLEMENTATION ===
void AAIDrone::UpdateVisualsAfterStateChange()
{
    // Called by the Character's Server RPC after setting the state.
    UpdateVisualFeedback();
}

void AAIDrone::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    OwningPC = Cast<APlayerController>(NewController);
    if (OwningPC && HasAuthority())
    {
        CurrentState = EDroneState::Possessed;
        SetOwner(NewController);
    }
}

void AAIDrone::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    
    // Add input mapping context on the client
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (PC->IsLocalController())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Subsystem->ClearAllMappings();
                Subsystem->AddMappingContext(DroneMappingContext, 2);
            }
        }
    }
}

void AAIDrone::UnPossessed()
{
    Super::UnPossessed();
    
    // Remove the drone's input mapping context on the client
    if (OwningPC && OwningPC->IsLocalController())
    {
        if (ULocalPlayer* LocalPlayer = OwningPC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
            {
                Subsystem->RemoveMappingContext(DroneMappingContext);
            }
        }
    }

    OwningPC = nullptr;

    if (HasAuthority())
    {
        CurrentState = EDroneState::Idle;
        FollowTarget = nullptr;
    }
}

void AAIDrone::OnRep_State()
{
    // This runs on clients when CurrentState replicates
    UpdateVisualFeedback();
}

void AAIDrone::UpdateVisualFeedback()
{
    if (DroneMesh)
    {
        UMaterialInstanceDynamic* MID = DroneMesh->CreateAndSetMaterialInstanceDynamic(0);
        if (MID)
        {
            FLinearColor Color;
            switch (CurrentState)
            {
            case EDroneState::Idle:     Color = FLinearColor::White; break;
            case EDroneState::Following: Color = FLinearColor::Blue;  break;
            case EDroneState::Possessed: Color = FLinearColor::Red;   break;
            }
            MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
        }
    }
}

void AAIDrone::MoveForward(const FInputActionValue& Value) 
{ 
    AddMovementInput(GetActorForwardVector(), Value.Get<float>()); 
}

void AAIDrone::MoveRight(const FInputActionValue& Value) 
{ 
    AddMovementInput(GetActorRightVector(), Value.Get<float>()); 
}

void AAIDrone::MoveUp(const FInputActionValue& Value) 
{ 
    AddMovementInput(FVector::UpVector, Value.Get<float>()); 
}

void AAIDrone::Turn(const FInputActionValue& Value) 
{ 
    AddControllerYawInput(Value.Get<float>()); 
}

void AAIDrone::LookUp(const FInputActionValue& Value) 
{ 
    AddControllerPitchInput(Value.Get<float>()); 
}

void AAIDrone::Unpossess(const FInputActionValue& Value)
{
    if (IsLocallyControlled())
    {
        ServerUnpossess();
    }
}

bool AAIDrone::ServerUnpossess_Validate()
{
    return true;
}

void AAIDrone::ServerUnpossess_Implementation()
{
    if (!OwningPC) return;
    
    AAIDronePlayerController* DronePC = Cast<AAIDronePlayerController>(OwningPC);
    if (!DronePC) return;
    
    // UnPossessed handles state change and cleanup now
    
    DronePC->PossessPreviousPawn();
}

bool AAIDrone::ServerMove_Validate(FVector ClientLocation, FVector InputVector, FRotator ControlRotation, float DeltaTime)
{
    return !ClientLocation.ContainsNaN() && !InputVector.ContainsNaN() && !ControlRotation.ContainsNaN();
}

void AAIDrone::ServerMove_Implementation(FVector ClientLocation, FVector InputVector, FRotator ControlRotation, float DeltaTime)
{
    FVector ServerLocation = GetActorLocation();
    float DistSq = FVector::DistSquared(ClientLocation, ServerLocation);
    const float MaxDistSq = 10000.0f;
    
    if (DistSq < MaxDistSq)
    {
        SetActorLocation(ClientLocation);
    }
    
    if (!InputVector.IsNearlyZero())
    {
        AddMovementInput(InputVector, 1.0f);
    }
    
    if (GetController())
    {
        GetController()->SetControlRotation(ControlRotation);
    }
}

bool AAIDrone::ServerRequestPossess_Validate(APlayerController* Requester) { return Requester != nullptr; }
void AAIDrone::ServerRequestPossess_Implementation(APlayerController* Requester)
{
    if (Requester && CurrentState != EDroneState::Possessed)
    {
        if (APawn* PlayerPawn = Requester->GetPawn())
        {
            if (FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation()) <= CommandRange)
            {
                Requester->Possess(this);
            }
        }
    }
}
#include "AIDrone.h"
#include "AIDronePlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "Engine/Engine.h"

AAIDrone::AAIDrone()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicatingMovement(true);

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();
    
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
        FRotator CurrentControlRotation = GetControlRotation();

        bool bRotationChanged = !CurrentControlRotation.Equals(LastSentRotation, 0.01f);
        
        if (!InputVector.IsNearlyZero() || GetVelocity().SizeSquared() > 1.0f || bRotationChanged)
        {
            ServerMove(GetActorLocation(), InputVector, GetControlRotation(), DeltaTime);
            LastSentRotation = CurrentControlRotation;
        }
    }

    // On server (authority), process movement for all pawns
   if (HasAuthority())
    {
        if (CurrentState == EDroneState::Possessed)
        {
            // Player-controlled. 
            // If the player is NOT pressing input (Input Vector is zero), we apply hover.
            if (GetLastMovementInputVector().IsNearlyZero())
            {
                ApplyHoverPhysics(DeltaTime);
            }
        }
        else if (CurrentState == EDroneState::Following && FollowTarget && FollowTarget->IsValidLowLevel())
        {
            FVector Dir = FollowTarget->GetActorLocation() - GetActorLocation();
            float Dist = Dir.Size();
            
            // If outside follow distance, MOVE
            if (Dist > FollowDistance)
            {
                // === Obstacle Avoidance Logic ===
                FVector AvoidanceVector = FVector::ZeroVector;
                float AvoidanceDistance = 300.0f;
                float AvoidanceForce = 1.0f;
                FHitResult HitResult;
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(this);
                
                if (GetWorld()->LineTraceSingleByChannel(HitResult, GetActorLocation(), GetActorLocation() + Dir.GetSafeNormal() * AvoidanceDistance, ECC_Visibility, Params))
                {
                    FVector HitNormal = HitResult.Normal;
                    if (FMath::Abs(HitNormal.Z) < 0.7f)
                    {
                        AvoidanceVector = FVector::CrossProduct(HitNormal, GetActorRightVector());
                        AvoidanceVector.Z += 0.5f; 
                    }
                    else
                    {
                        AvoidanceVector = GetActorRightVector();
                    }
                    AvoidanceVector = AvoidanceVector.GetSafeNormal() * AvoidanceForce;
                }
                // ================================

                FVector TargetDirection = Dir.GetSafeNormal();
                FVector FinalMoveDirection = (TargetDirection + AvoidanceVector).GetSafeNormal();

                // Rotate
                FRotator TargetRot = FinalMoveDirection.Rotation();
                TargetRot.Pitch = 0.0f;
                TargetRot.Roll = 0.0f;
                FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 8.0f);
                SetActorRotation(NewRot);
                
                // Move
                float MovementMagnitude = FMath::Clamp((Dist - FollowDistance) / FollowDistance, 0.1f, 1.0f);
                AddMovementInput(FinalMoveDirection, MovementMagnitude);
            }
            else 
            {
                // If inside follow distance (Stationary), HOVER
                ApplyHoverPhysics(DeltaTime);
            }
        }
        else if (CurrentState == EDroneState::Idle)
        {
            // Always hover in idle
            ApplyHoverPhysics(DeltaTime);
        }
    }
}

// === NEW HELPER FUNCTION ===
void AAIDrone::ApplyHoverPhysics(float DeltaTime)
{
    float Time = GetWorld()->GetTimeSeconds();
    
    // Calculate Sine wave for Current Time and Previous Frame Time
    // This stateless approach prevents "spikes" when switching states
    float CurrentHoverHeight = FMath::Sin(Time * HoverFrequency) * HoverAmplitude;
    float PreviousHoverHeight = FMath::Sin((Time - DeltaTime) * HoverFrequency) * HoverAmplitude;
    
    // Derive Velocity required to move from Previous to Current height
    float HoverVelZ = (CurrentHoverHeight - PreviousHoverHeight) / DeltaTime;

    // Apply scaled input
    if (MovementComponent)
    {
        AddMovementInput(FVector::UpVector, HoverVelZ / MovementComponent->MaxSpeed);
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
        
        // Spawn AI Controller so the drone doesn't freeze
        SpawnDefaultController();
    }
}

void AAIDrone::OnRep_State()
{
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

    SetActorRotation(ControlRotation);
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
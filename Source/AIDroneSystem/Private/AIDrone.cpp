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

    // === CRITICAL FIX: Properly handle replicated movement ===
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
        // If possessed, don't do autonomous behavior
        if (GetController() != nullptr)
        {
            // Server processes movement for possessed pawns
            // Movement is already handled by AddMovementInput calls from input
        }
        else
        {
            // Autonomous behavior when not possessed
            if (CurrentState == EDroneState::Following && FollowTarget && FollowTarget->IsValidLowLevel())
            {
                FVector Dir = FollowTarget->GetActorLocation() - GetActorLocation();
                float Dist = Dir.Size();
                if (Dist > FollowDistance)
                {
                    Dir = Dir.GetSafeNormal();
                    FRotator TargetRot = Dir.Rotation();
                    TargetRot.Pitch = 0.0f;
                    TargetRot.Roll = 0.0f;
                    FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 8.0f);
                    SetActorRotation(NewRot);
                    AddMovementInput(Dir, 1.0f);
                }
            }
            else if (CurrentState == EDroneState::Idle)
            {
                float Time = GetWorld()->GetTimeSeconds();
                float HoverOffset = FMath::Sin(Time * HoverFrequency) * HoverAmplitude;
                float HoverVelZ = (HoverOffset - LastHoverOffset) / DeltaTime;
                AddMovementInput(FVector::UpVector, HoverVelZ / MovementComponent->MaxSpeed);
                LastHoverOffset = HoverOffset;
                AddMovementInput(GetActorForwardVector(), 0.2f);
            }
        }
    }
    // =========================================================
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

void AAIDrone::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    OwningPC = Cast<APlayerController>(NewController);
    if (OwningPC && HasAuthority())
    {
        CurrentState = EDroneState::Possessed;
        SetOwner(NewController);
        
        UE_LOG(LogTemp, Warning, TEXT("Drone possessed by controller on Server"));
    }
}

void AAIDrone::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    
    UE_LOG(LogTemp, Warning, TEXT("Drone OnRep_PlayerState called - Adding input context"));
    
    // This runs on the client when PlayerState replicates
    // Add input mapping context here for the client
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (PC->IsLocalController())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                // Clear all contexts first to avoid duplicates
                Subsystem->ClearAllMappings();
                
                // Add drone's input mapping context
                Subsystem->AddMappingContext(DroneMappingContext, 2);
                UE_LOG(LogTemp, Warning, TEXT("Drone - Added input mapping context on client"));
            }
        }
    }
}

void AAIDrone::UnPossessed()
{
    Super::UnPossessed();
    
    UE_LOG(LogTemp, Warning, TEXT("Drone UnPossessed called"));
    
    // Remove the drone's input mapping context on the client
    if (OwningPC && OwningPC->IsLocalController())
    {
        if (ULocalPlayer* LocalPlayer = OwningPC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
            {
                Subsystem->RemoveMappingContext(DroneMappingContext);
                UE_LOG(LogTemp, Warning, TEXT("Removed drone mapping context"));
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

// Input handlers use AddMovementInput which accumulates in APawn's internal vector
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
    // Call server RPC to unpossess
    if (IsLocallyControlled())
    {
        ServerUnpossess();
    }
}

// === Server RPC for unpossession ===
bool AAIDrone::ServerUnpossess_Validate()
{
    return true;
}

void AAIDrone::ServerUnpossess_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("Server: ServerUnpossess called"));
    
    if (!OwningPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server: No OwningPC, aborting unpossess"));
        return;
    }
    
    // Cast to our custom controller
    AAIDronePlayerController* DronePC = Cast<AAIDronePlayerController>(OwningPC);
    if (!DronePC)
    {
        UE_LOG(LogTemp, Error, TEXT("Server: PlayerController is not AAIDronePlayerController! Make sure to set it in GameMode."));
        return;
    }
    
    // Clear member variables
    OwningPC = nullptr;
    
    // Update drone state
    CurrentState = EDroneState::Idle;
    FollowTarget = nullptr;
    
    // Use the custom controller's method to switch back
    DronePC->PossessPreviousPawn();
}
// ===================================

// === CRITICAL FIX: Server RPC to sync location, input and rotation ===
bool AAIDrone::ServerMove_Validate(FVector ClientLocation, FVector InputVector, FRotator ControlRotation, float DeltaTime)
{
    // Basic validation - check for reasonable values
    return !ClientLocation.ContainsNaN() && !InputVector.ContainsNaN() && !ControlRotation.ContainsNaN();
}

void AAIDrone::ServerMove_Implementation(FVector ClientLocation, FVector InputVector, FRotator ControlRotation, float DeltaTime)
{
    // Server accepts client's location with some validation
    // In a production game, you'd want to validate this more strictly
    FVector ServerLocation = GetActorLocation();
    float DistSq = FVector::DistSquared(ClientLocation, ServerLocation);
    
    // If client location is too far from server (possible cheat/desync), correct it
    // Otherwise, trust the client and update server location
    const float MaxDistSq = 10000.0f; // 100 units squared
    
    if (DistSq < MaxDistSq)
    {
        // Accept client location
        SetActorLocation(ClientLocation);
    }
    
    // Apply input on server
    if (!InputVector.IsNearlyZero())
    {
        AddMovementInput(InputVector, 1.0f);
    }
    
    // Apply rotation on server
    if (GetController())
    {
        GetController()->SetControlRotation(ControlRotation);
    }
}
// ======================================================================

// RPCs (unchanged)
bool AAIDrone::ServerFollowMe_Validate(ACharacter* Player) { return Player != nullptr; }
void AAIDrone::ServerFollowMe_Implementation(ACharacter* Player)
{
    if (Player && FVector::Dist(GetActorLocation(), Player->GetActorLocation()) <= CommandRange)
    {
        FollowTarget = Player;
        CurrentState = EDroneState::Following;
        UpdateVisualFeedback();
    }
}

bool AAIDrone::ServerUnfollow_Validate() { return true; }
void AAIDrone::ServerUnfollow_Implementation()
{
    FollowTarget = nullptr;
    CurrentState = EDroneState::Idle;
    UpdateVisualFeedback();
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
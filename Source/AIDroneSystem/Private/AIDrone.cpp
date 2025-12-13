#include "AIDrone.h"
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

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    Root->SetIsReplicated(true);

    DroneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneMesh"));
    DroneMesh->SetupAttachment(Root);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Root);  // Fixed: Removed invalid 'FName(), false' args
    Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));

    MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
    MovementComponent->UpdatedComponent = Root;
    MovementComponent->MaxSpeed = 800.0f;
    MovementComponent->SetIsReplicated(true);
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

    if (HasAuthority() && GetController() == nullptr)
    {
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
            // Slow drift
            AddMovementInput(GetActorForwardVector(), 0.2f);
        }
    }
}

void AAIDrone::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // === CRITICAL FIX: Add the mapping context here, as this is the engine's input setup point. ===
        // This runs reliably on the owning client when PossessedBy is called.
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
                {
                    // Remove the old context (if any) and add the new one.
                    // Assuming the player character's context is at priority 0. Drone should be higher.
                    Subsystem->AddMappingContext(DroneMappingContext, 2); 
                }
            }
        }
        // ==============================================================================================

        EnhancedInputComponent->BindAction(DroneForwardAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveForward);
        EnhancedInputComponent->BindAction(DroneRightAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveRight);
        EnhancedInputComponent->BindAction(DroneUpAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveUp);
        EnhancedInputComponent->BindAction(DroneYawAction, ETriggerEvent::Triggered, this, &AAIDrone::Turn);
        EnhancedInputComponent->BindAction(DronePitchAction, ETriggerEvent::Triggered, this, &AAIDrone::LookUp);
        EnhancedInputComponent->BindAction(UnpossessAction, ETriggerEvent::Triggered, this, &AAIDrone::Unpossess);
    }

    // if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    // {
    //     EnhancedInputComponent->BindAction(DroneForwardAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveForward);
    //     EnhancedInputComponent->BindAction(DroneRightAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveRight);
    //     EnhancedInputComponent->BindAction(DroneUpAction, ETriggerEvent::Triggered, this, &AAIDrone::MoveUp);
    //     EnhancedInputComponent->BindAction(DroneYawAction, ETriggerEvent::Triggered, this, &AAIDrone::Turn);
    //     EnhancedInputComponent->BindAction(DronePitchAction, ETriggerEvent::Triggered, this, &AAIDrone::LookUp);
    //     EnhancedInputComponent->BindAction(UnpossessAction, ETriggerEvent::Triggered, this, &AAIDrone::Unpossess);
    // }
}

void AAIDrone::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    OwningPC = Cast<APlayerController>(NewController);
    if (OwningPC)
    {
        // This block runs on the SERVER when possession occurs.
        if (HasAuthority())
        {
            CurrentState = EDroneState::Possessed;
            SetOwner(NewController);
            // === CRITICAL FIX: Grant Movement Component Authority to the Controller ===
            // This tells the engine that the owning client is responsible for movement, 
            // and the movement data must be sent back to the server.
            if (MovementComponent)
            {
                // The UFloatingPawnMovement component must be replicated for client movement to work.
                // You already have SetIsReplicated(true) in the constructor, but it's safe to ensure here.
                MovementComponent->SetIsReplicated(true);
                
                
                // SetComponentOwner is usually handled by the possession system for Pawns, 
                // but setting movement component replication is essential.
            }
        }
    }
}

void AAIDrone::UnPossessed()
{
    Super::UnPossessed();

    if (OwningPC)
    {
        // === CRITICAL FIX: Only remove the input context if the Pawn was LOCALLY CONTROLLED ===
        // if (OwningPC->IsLocalController())
        // {
        //     if (ULocalPlayer* LocalPlayer = OwningPC->GetLocalPlayer())
        //     {
        //         if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
        //         {
        //             Subsystem->RemoveMappingContext(DroneMappingContext);
        //         }
        //     }
        // }
        // ====================================================================================

        OwningPC = nullptr;
    }

    if (HasAuthority())
    {
        // State update is only done by the server
        CurrentState = EDroneState::Idle;
        FollowTarget = nullptr;
        // State replication (OnRep_State) will call UpdateVisualFeedback on all clients
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

// Input Implementations (unchanged)
void AAIDrone::MoveForward(const FInputActionValue& Value) { AddMovementInput(GetActorForwardVector(), Value.Get<float>()); UE_LOG(LogTemp, Warning, TEXT("Moveforward_Backward")) }
void AAIDrone::MoveRight(const FInputActionValue& Value) { AddMovementInput(GetActorRightVector(), Value.Get<float>()); }
void AAIDrone::MoveUp(const FInputActionValue& Value) { AddMovementInput(FVector::UpVector, Value.Get<float>()); }
void AAIDrone::Turn(const FInputActionValue& Value) { AddControllerYawInput(Value.Get<float>()); UE_LOG(LogTemp, Warning, TEXT("Turn")); }
void AAIDrone::LookUp(const FInputActionValue& Value) { AddControllerPitchInput(Value.Get<float>()); }
void AAIDrone::Unpossess(const FInputActionValue& Value)
{
    if (OwningPC) OwningPC->UnPossess();
}

// RPCs (unchanged)
bool AAIDrone::ServerFollowMe_Validate(ACharacter* Player) { return Player != nullptr; }
void AAIDrone::ServerFollowMe_Implementation(ACharacter* Player)
{
    UE_LOG(LogTemp, Display, TEXT("ServerFollowMe_Implementation Begin"));
    if (Player && FVector::Dist(GetActorLocation(), Player->GetActorLocation()) <= CommandRange)
    {
        FollowTarget = Player;
        CurrentState = EDroneState::Following;
        UpdateVisualFeedback();
        UE_LOG(LogTemp, Display, TEXT("ServerFollowMe_Implementation"));
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
    UE_LOG(LogTemp, Display, TEXT("ServerRequestPossess_Implementation"));
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
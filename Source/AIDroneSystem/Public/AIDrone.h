#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Net/UnrealNetwork.h"
#include "AIDrone.generated.h"

UENUM(BlueprintType)
enum class EDroneState : uint8
{
    Idle,
    Following,
    Possessed
};

UCLASS()
class AIDRONESYSTEM_API AAIDrone : public APawn
{
    GENERATED_BODY()

public:
    AAIDrone();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerFollowMe(ACharacter* Player);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUnfollow();

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerRequestPossess(APlayerController* Requester);

    // === CRITICAL FIX: Add RPC to replicate movement input ===
    UFUNCTION(Server, Unreliable, WithValidation)
    void ServerMove(FVector ClientLocation, FVector InputVector, FRotator ControlRotation, float DeltaTime);
    
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUnpossess();
    // =========================================================

    UPROPERTY(ReplicatedUsing = OnRep_State)
    EDroneState CurrentState;

    UPROPERTY(Replicated)
    ACharacter* FollowTarget;
    
    // Behavior params
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float CommandRange = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float FollowDistance = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float HoverAmplitude = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float HoverFrequency = 1.0f;

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;
    virtual void OnRep_PlayerState() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_State();

    void UpdateVisualFeedback();

    // Drone Inputs
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DroneMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* DroneForwardAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* DroneRightAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* DroneUpAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* DroneYawAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* DronePitchAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* UnpossessAction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UCameraComponent* Camera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UStaticMeshComponent* DroneMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UFloatingPawnMovement* MovementComponent;

    UPROPERTY()
    APlayerController* OwningPC;

private:
    // Input handlers
    void MoveForward(const FInputActionValue& Value);
    void MoveRight(const FInputActionValue& Value);
    void MoveUp(const FInputActionValue& Value);
    void Turn(const FInputActionValue& Value);
    void LookUp(const FInputActionValue& Value);
    void Unpossess(const FInputActionValue& Value);

    float LastHoverOffset = 0.0f;
};
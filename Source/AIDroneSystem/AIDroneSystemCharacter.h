// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "AIDroneSystemCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class AAIDrone;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AAIDroneSystemCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

public:
	AAIDroneSystemCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
			

protected:

	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// virtual void OnRep_PlayerState() override;

	// virtual void PossessedBy(AController* NewController) override;


public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UPROPERTY(EditAnywhere, Replicated, Transient)
	TObjectPtr<AAIDrone> AIDrone;
	
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputAction> FollowDrone;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputAction> UnFollowDrone;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputAction> PossessDrone;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputAction> DroneInteraction;

	UPROPERTY(EditDefaultsOnly)
	TEnumAsByte<ECollisionChannel> DroneInteractionChannel;

	UPROPERTY(EditDefaultsOnly)
	double TraceLength;


	void DroneFollowMe();
	void DroneUnfollowMe();
	void PossessDroneRequest();
	void InteractDroneRequest();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPossessDrone(AAIDrone* DroneToPossess, APlayerController* Requester);
};


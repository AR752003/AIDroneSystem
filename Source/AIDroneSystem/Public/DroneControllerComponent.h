// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneControllerComponent.generated.h"


class UInputAction;
class UInputMappingContext;
class AAIDrone;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AIDRONESYSTEM_API UDroneControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UDroneControllerComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Replicated, Transient)
	TObjectPtr<AAIDrone> AIDrone;

	UPROPERTY()
	TObjectPtr<ACharacter> DroneOwningCharacter;

	UPROPERTY(EditDefaultsOnly)
	UInputMappingContext* InputMappingContext;

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


	
	void InitializeDroneControllerComponent();
	void DroneFollowMe();
	void DroneUnfollowMe();
	void PossessDroneRequest();
	void InteractDroneRequest();


	
};

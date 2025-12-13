// Fill out your copyright notice in the Description page of Project Settings.


#include "DroneControllerComponent.h"
#include "Net/UnrealNetwork.h"
#include "AIDrone.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"


// Sets default values for this component's properties
UDroneControllerComponent::UDroneControllerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	// ...
}

void UDroneControllerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate AIDrone to all clients (Autonomous Proxy and Simulated Proxy)
	DOREPLIFETIME(UDroneControllerComponent, AIDrone);
}

// Called when the game starts
void UDroneControllerComponent::BeginPlay()
{
	Super::BeginPlay();
	// InitializeDroneControllerComponent();
	// ...
	
}


// Called every frame
void UDroneControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UDroneControllerComponent::InitializeDroneControllerComponent()
{
	DroneOwningCharacter = Cast<ACharacter>(GetOwner());
	if (!IsValid(DroneOwningCharacter)) return;
	UE_LOG(LogTemp, Warning, TEXT("Drone Character Is Valid"));
	APlayerController* PlayerController = DroneOwningCharacter->GetController<APlayerController>();
	if (!IsValid(PlayerController)) return;
	UE_LOG(LogTemp, Warning, TEXT("Drone player Controller Is Valid"));
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	if (!IsValid(Subsystem)) return;
	UE_LOG(LogTemp, Warning, TEXT("Subsystem is valid"));
	if (!IsValid(InputMappingContext)) return;
	UE_LOG(LogTemp, Warning, TEXT("IMC is valid"));
	Subsystem->AddMappingContext(InputMappingContext,0);
	UE_LOG(LogTemp, Warning, TEXT("Added InputMappingContext: %s"), *InputMappingContext->GetName());
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(DroneOwningCharacter->InputComponent);
	if (!IsValid(EnhancedInputComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("InitializeDroneControllerComponent failed: EnhancedInputComponent is NULL. Input binding skipped."));
		// Returning here prevents the crash on the next lines.
		return; 
	}
	UE_LOG(LogTemp, Warning, TEXT("EnhancedInputIsValid"));
	EnhancedInputComponent->BindAction(FollowDrone, ETriggerEvent::Started, this, &UDroneControllerComponent::DroneFollowMe);
	EnhancedInputComponent->BindAction(UnFollowDrone, ETriggerEvent::Started, this, &UDroneControllerComponent::DroneUnfollowMe);
	EnhancedInputComponent->BindAction(PossessDrone, ETriggerEvent::Started, this, &UDroneControllerComponent::PossessDroneRequest);
	EnhancedInputComponent->BindAction(DroneInteraction, ETriggerEvent::Started, this, &UDroneControllerComponent::InteractDroneRequest);

	
	
}

void UDroneControllerComponent::DroneFollowMe()
{
	UE_LOG(LogTemp, Display, TEXT("DroneFollowMe Begin"));
	// === MODIFIED: Null Check to prevent crash ===
	if (AIDrone && AIDrone->IsValidLowLevel())
	{
		AIDrone->ServerFollowMe(DroneOwningCharacter);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneFollowMe failed: AIDrone is NULL!"));
	}
}

void UDroneControllerComponent::DroneUnfollowMe()
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

void UDroneControllerComponent::PossessDroneRequest()
{
	UE_LOG(LogTemp, Display, TEXT("Possess Drone Request"));
	// === MODIFIED: Null Check to prevent crash ===
	if (AIDrone && AIDrone->IsValidLowLevel())
	{
		APlayerController* PlayerController = DroneOwningCharacter->GetController<APlayerController>();
		AIDrone->ServerRequestPossess(PlayerController);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PossessDroneRequest failed: AIDrone is NULL!"));
	}
}

void UDroneControllerComponent::InteractDroneRequest()
{
	// NOTE: This function only runs on the client that owns the component (it's called by a local input). 
	// Since AIDrone is Replicated, setting it here on the owning client will automatically replicate it 
	// to the server and all other clients. The Server-Client Interaction flow is correct here.
	FVector2D ViewportSize;
	GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
	FVector2D ViewportCenter = ViewportSize/2;
	FVector TraceStart;
	FVector Forward;
	APlayerController* PlayerController = DroneOwningCharacter->GetController<APlayerController>();
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




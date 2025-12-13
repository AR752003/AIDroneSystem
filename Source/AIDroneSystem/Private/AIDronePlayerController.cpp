#include "AIDronePlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"

AAIDronePlayerController::AAIDronePlayerController()
{
	PreviousPawn = nullptr;
}

void AAIDronePlayerController::OnPossess(APawn* InPawn)
{
	// Store the current pawn BEFORE calling Super (which changes GetPawn())
	APawn* CurrentPawn = GetPawn();
	
	// Only store if we have a current pawn and it's different from the one we're about to possess
	if (CurrentPawn && CurrentPawn != InPawn)
	{
		PreviousPawn = CurrentPawn;
		UE_LOG(LogTemp, Warning, TEXT("PlayerController: Storing previous pawn: %s"), *PreviousPawn->GetName());
	}
	
	// Call parent OnPossess
	Super::OnPossess(InPawn);
	
	UE_LOG(LogTemp, Warning, TEXT("PlayerController: Now possessing: %s"), InPawn ? *InPawn->GetName() : TEXT("None"));
}

void AAIDronePlayerController::PossessPreviousPawn()
{
	if (PreviousPawn && IsValid(PreviousPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerController: Switching back to previous pawn: %s"), *PreviousPawn->GetName());
		
		// Store current as temp before switching
		APawn* Temp = PreviousPawn;
		PreviousPawn = nullptr; // Clear to avoid circular reference
		
		Possess(Temp);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerController: No valid previous pawn to possess!"));
	}
}
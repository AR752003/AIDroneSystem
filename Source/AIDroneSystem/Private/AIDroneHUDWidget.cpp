#include "AIDroneHUDWidget.h"
#include "AIDroneSystem/AIDroneSystemCharacter.h"
#include "AIDrone.h"
#include "GameFramework/PlayerController.h"

// Helper function to get the actual Drone actor we want to display
AAIDrone* GetTargetDrone(APawn* OwningPawn)
{
	// Case 1: The player is controlling the Drone directly
	if (AAIDrone* ControlledDrone = Cast<AAIDrone>(OwningPawn))
	{
		return ControlledDrone;
	}
    
	// Case 2: The player is controlling the Character, get the linked drone
	if (AAIDroneSystemCharacter* Character = Cast<AAIDroneSystemCharacter>(OwningPawn))
	{
		return Character->AIDrone;
	}

	return nullptr;
}

FText UAIDroneHUDWidget::GetDroneNameText() const
{
	AAIDrone* Drone = GetTargetDrone(GetOwningPlayerPawn());

	if (Drone)
	{
		return FText::FromString(Drone->GetName());
	}

	return FText::FromString(TEXT("No Drone Linked"));
}

FText UAIDroneHUDWidget::GetDroneStateText() const
{
	AAIDrone* Drone = GetTargetDrone(GetOwningPlayerPawn());

	if (Drone)
	{
		switch (Drone->CurrentState)
		{
		case EDroneState::Idle:
			return FText::FromString(TEXT("Status: Idle"));
		case EDroneState::Following:
			return FText::FromString(TEXT("Status: Following"));
		case EDroneState::Possessed:
			return FText::FromString(TEXT("Status: Player Controlled"));
		default:
			return FText::FromString(TEXT("Status: Unknown"));
		}
	}

	return FText::FromString(TEXT("Status: --"));
}

// You can remove the old GetOwningDroneCharacter helper or leave it unused.
AAIDroneSystemCharacter* UAIDroneHUDWidget::GetOwningDroneCharacter() const
{
	APawn* OwningPawn = GetOwningPlayerPawn();
	return Cast<AAIDroneSystemCharacter>(OwningPawn);
}
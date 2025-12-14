#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIDroneHUDWidget.generated.h"

// Forward Declarations
class AAIDroneSystemCharacter;

/**
 * Widget class to display AIDrone information for the local player.
 */
UCLASS()
class AIDRONESYSTEM_API UAIDroneHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** * Returns the name of the currently linked Drone actor.
	 * Bind this function to a Text Block in UMG.
	 */
	UFUNCTION(BlueprintPure, Category = "Drone Info")
	FText GetDroneNameText() const;

	/** * Returns the current state (Idle, Following, Possessed) of the linked Drone.
	 * Bind this function to a Text Block in UMG.
	 */
	UFUNCTION(BlueprintPure, Category = "Drone Info")
	FText GetDroneStateText() const;

private:
	/** Helper to get the correct character cast from the owning player */
	AAIDroneSystemCharacter* GetOwningDroneCharacter() const;
};
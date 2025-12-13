#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AIDronePlayerController.generated.h"

/**
 * Custom Player Controller that tracks pawn possession for drone switching
 */
UCLASS()
class AIDRONESYSTEM_API AAIDronePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAIDronePlayerController();

	// Override OnPossess to track the previous pawn
	virtual void OnPossess(APawn* InPawn) override;

	// Get the pawn that was possessed before the current one
	FORCEINLINE APawn* GetPreviousPawn() const { return PreviousPawn; }

	// Switch back to the previous pawn
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void PossessPreviousPawn();

protected:
	// Stores the pawn that was possessed before the current one
	UPROPERTY()
	APawn* PreviousPawn;
};
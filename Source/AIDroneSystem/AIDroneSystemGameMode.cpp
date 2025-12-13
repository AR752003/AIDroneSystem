// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIDroneSystemGameMode.h"
#include "AIDroneSystemCharacter.h"
#include "UObject/ConstructorHelpers.h"

AAIDroneSystemGameMode::AAIDroneSystemGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

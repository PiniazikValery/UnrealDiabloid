// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyProjectCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "AI/NavigationSystemBase.h"
#include "NavigationSystem.h"
#include "./AI/MyAIController.h"
#include "./AI/AEnemySpawner.h"
#include "MyProjectGameMode.generated.h"

UCLASS(minimalapi)
class AMyProjectGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMyProjectGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	void SpawnCharacterAtReachablePointTest();
	void SpawnLandscapeGenerator();
	void SetupNavigation();
	void SpawnEnemySpawner();
	void SpawnWarmupManager();
};

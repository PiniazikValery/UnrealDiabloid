// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyProjectCharacter.h"
#include "MyProjectPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "AI/NavigationSystemBase.h"
#include "NavigationSystem.h"
#include "./AI/MyAIController.h"
#include "./Mass/MassEnemySpawner.h"
#include "MyProjectGameMode.generated.h"

UCLASS(minimalapi)
class AMyProjectGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMyProjectGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

protected:
	UFUNCTION()
	void EnsurePlayerControllerInputComponent(APlayerController* PlayerController);

private:
	void SpawnCharacterAtReachablePointTest();
	void SpawnLandscapeGenerator();
	void SetupNavigation();
	void SpawnEnemySpawner();
	void SpawnWarmupManager();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../MyProjectCharacter.h"
#include "../EnemyCharacter.h"
#include "./MyAIController.h"
#include "AEnemySpawner.generated.h"

UCLASS()
class MYPROJECT_API AAEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AAEnemySpawner();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	int32 MaxEnemies = 50;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	int32 EnemiesPerWave = 3;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	float SpawnRadius = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	float SpawnInterval = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	TSubclassOf<AMyProjectCharacter> EnemyClass = AEnemyCharacter::StaticClass();

	FTimerHandle SpawnTimerHandle;

	int32 ActiveEnemyCount = 0;

	void SpawnWave();
	void SpawnSingleEnemy(EEnemyType Type);
	UFUNCTION()
	void OnEnemyDestroyed(AActor* DestroyedEnemy);
};
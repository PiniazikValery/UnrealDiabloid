// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassEntityTypes.h"
#include "EnemySpawner.generated.h"

class UMassEntityConfigAsset;

/**
 * Actor that spawns Mass entities (enemies)
 * Place this in your level or spawn from GameMode
 */
UCLASS()
class MYPROJECT_API AEnemySpawner : public AActor
{
	GENERATED_BODY()
	
public:    
	AEnemySpawner();

	// Enemy configuration data asset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning")
	UMassEntityConfigAsset* EnemyEntityConfig;

	// Number of enemies to spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 NumEnemiesToSpawn = 300;

	// Spawn area radius
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning", meta = (ClampMin = "100.0", ClampMax = "50000.0"))
	float SpawnRadius = 5000.0f;

	// Spawn center offset from this actor's location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning")
	FVector SpawnCenterOffset = FVector::ZeroVector;

	// Auto-spawn on BeginPlay?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning")
	bool bAutoSpawnOnBeginPlay = true;

	// Spawn height offset (prevents spawning underground)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Spawning")
	float SpawnHeightOffset = 100.0f;

	// Spawn enemies function
	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning")
	void SpawnEnemies();

	// Despawn all spawned enemies
	UFUNCTION(BlueprintCallable, Category = "Enemy Spawning")
	void DespawnAllEnemies();

	// Get number of currently spawned enemies
	UFUNCTION(BlueprintPure, Category = "Enemy Spawning")
	int32 GetSpawnedEnemyCount() const { return SpawnedEntities.Num(); }

protected:
	virtual void BeginPlay() override;

private:
	// Array of spawned entity handles for tracking
	TArray<FMassEntityHandle> SpawnedEntities;
};

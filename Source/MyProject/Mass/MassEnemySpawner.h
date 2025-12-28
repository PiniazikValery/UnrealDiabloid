// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassEntityTypes.h"
#include "MassEnemySpawner.generated.h"

// Forward declarations
class UMassEntityConfigAsset;
class UMassEntitySubsystem;
class UInstancedStaticMeshComponent;
struct FMassEntityTemplate;

/**
 * Mass-based enemy spawner
 * 
 * Replaces Actor-based spawning system with Mass entities for massive performance gains.
 * 
 * Key Features:
 * - Wave-based spawning (like original spawner)
 * - Max enemy limit enforcement
 * - Navigation-aware spawn points
 * - Automatic visualization management
 * - 10-50x better performance than Actor-based
 * 
 * Usage:
 * 1. Place in level
 * 2. Assign EnemyEntityConfig (DA_EnemyEntity)
 * 3. Configure spawn parameters
 * 4. Play - enemies spawn automatically
 */
UCLASS()
class MYPROJECT_API AMassEnemySpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	AMassEnemySpawner();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ========================================
	// CONFIGURATION (mirrors original spawner)
	// ========================================
	
	/**
	 * Mass entity configuration asset
	 * Equivalent to: EnemyClass in original spawner
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Core")
	TObjectPtr<UMassEntityConfigAsset> EnemyEntityConfig;

	/**
	 * Number of enemies to spawn per wave
	 * Equivalent to: EnemiesPerWave in original spawner
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Waves", meta = (ClampMin = "1", ClampMax = "100"))
	int32 EnemiesPerWave = 10;

	/**
	 * Maximum number of enemies alive at once
	 * Equivalent to: MaxEnemies in original spawner
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Limits", meta = (ClampMin = "1", ClampMax = "2000"))
	int32 MaxEnemies = 300;

	/**
	 * Time between spawn waves (seconds)
	 * Equivalent to: SpawnInterval in original spawner
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Waves", meta = (ClampMin = "0.1", ClampMax = "60.0"))
	float SpawnInterval = 5.0f;

	/**
	 * Spawn radius around player (units)
	 * Equivalent to: SpawnRadius in original spawner
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Location", meta = (ClampMin = "500", ClampMax = "10000"))
	float SpawnRadius = 2000.0f;

	/**
	 * Minimum spawn distance from player (prevents spawning too close)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Location", meta = (ClampMin = "0", ClampMax = "5000"))
	float MinSpawnDistance = 500.0f;

	/**
	 * Use navigation system for spawn point validation?
	 * If true: spawns only on navigable surfaces (like original spawner)
	 * If false: spawns at any random point (faster, but may spawn in walls)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Location")
	bool bUseNavigationSystem = true;

	/**
	 * Auto-start spawning on BeginPlay?
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Core")
	bool bAutoSpawnOnBeginPlay = true;

	// ========================================
	// VISUALIZATION (optional - for debugging)
	// ========================================
	
	/**
	 * Static mesh for enemy visualization
	 * If not set, uses mesh from EnemyVisualizationProcessor
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Visuals")
	TObjectPtr<UStaticMesh> EnemyMesh;

	/**
	 * Material override for enemies
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Spawning|Visuals")
	TObjectPtr<UMaterialInterface> EnemyMaterial;

	// ========================================
	// RUNTIME CONTROL (Blueprint callable)
	// ========================================
	
	/**
	 * Start wave-based spawning
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass Spawning")
	void StartSpawning();

	/**
	 * Stop wave-based spawning (doesn't despawn existing enemies)
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass Spawning")
	void StopSpawning();

	/**
	 * Spawn a single wave immediately
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass Spawning")
	void SpawnWave();

	/**
	 * Spawn single enemy at specific location
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass Spawning")
	void SpawnSingleEnemy(const FVector& Location);

	/**
	 * Destroy all spawned enemies and clear instances
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass Spawning")
	void DespawnAllEnemies();

	/**
	 * Get current number of active enemies
	 * Equivalent to: ActiveEnemyCount in original spawner
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mass Spawning")
	int32 GetActiveEnemyCount() const { return SpawnedEntities.Num(); }

	/**
	 * Check if spawning is currently active
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mass Spawning")
	bool IsSpawning() const;

	/**
	 * Get all spawned entity handles (server-side only)
	 * Useful for targeting systems that need to iterate through all enemies
	 */
	const TArray<FMassEntityHandle>& GetSpawnedEntities() const { return SpawnedEntities; }

private:
	// ========================================
	// INTERNAL STATE
	// ========================================
	
	// Timer handle for wave spawning (like original SpawnTimerHandle)
	FTimerHandle SpawnTimerHandle;
	
	// Track all spawned Mass entities (replaces tracking Actor pointers)
	TArray<FMassEntityHandle> SpawnedEntities;
	
	// ISM component for visualization (optional - processor can handle this)
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> VisualizationISM;
	
	// Cached player reference (performance optimization)
	TWeakObjectPtr<APawn> CachedPlayerPawn;
	
	// Entity template cache (avoid recreating every spawn)
	const FMassEntityTemplate* CachedEntityTemplate;

	// ========================================
	// HELPER FUNCTIONS
	// ========================================
	
	/**
	 * Create Mass entity at location with initial setup
	 * Returns entity handle (invalid if failed)
	 */
	FMassEntityHandle CreateMassEntity(const FVector& Location);
	
	/**
	 * Setup optional visualization ISM component
	 * Called once in BeginPlay
	 */
	void SetupVisualization();
	
	/**
	 * Get random spawn location around player
	 * Uses navigation system if enabled
	 * Returns true if valid location found
	 */
	bool GetSpawnLocation(FVector& OutLocation);
	
	/**
	 * Validate and get player pawn reference
	 */
	APawn* GetPlayerPawn();
	
	/**
	 * Cleanup invalid entities from tracking array
	 */
	void CleanupInvalidEntities();
};

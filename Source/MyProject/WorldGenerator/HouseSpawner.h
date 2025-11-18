// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HouseSpawner.generated.h"

UCLASS()
class MYPROJECT_API AHouseSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AHouseSpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	// House spawning system
	UPROPERTY()
	TArray<UInstancedStaticMeshComponent*> HouseInstancedMeshes;
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	TArray<UStaticMesh*> HouseMeshes;
	
	// Track houses per tile for cleanup
	TMap<FIntPoint, TArray<FTransform>> TileHouseTransforms;
	
	// House spawning parameters
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	float HouseSpawnChance = 0.3f; // 30% chance per tile
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	int32 MinHousesPerTile = 1;
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	int32 MaxHousesPerTile = 3;
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	float MinHouseDistance = 1000.0f; // Minimum distance between houses
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	float HouseScaleMin = 0.9f;
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	float HouseScaleMax = 1.1f;
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	float SpawnDistanceInFrontOfPlayer = 500.0f;
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	float HouseBoundsRadius = 300.0f; // Approximate radius for corner sampling
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	float HousePivotZOffset = 30.0f; // Offset from pivot to base (negative = pivot above base)
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	bool bAlignToTerrainSlope = true; // Rotate house to match terrain
	
	UPROPERTY(EditAnywhere, Category = "House Spawning")
	bool bUseLowestPoint = true; // Use lowest corner instead of average
	
	// House spawning methods
	void SpawnHousesOnTile(const FIntPoint& TileKey);
	void RemoveHousesOnTile(const FIntPoint& TileKey);
	void SpawnHouseInFrontOfPlayer();
	
	// Helper methods
	FVector GetRandomPointOnTile(const FIntPoint& TileKey);
	bool IsValidHousePosition(const FVector& Position, const TArray<FVector>& ExistingHouses);
};

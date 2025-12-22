// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NavBlockerActor.generated.h"

UCLASS()
class MYPROJECT_API ANavBlockerActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ANavBlockerActor();

	// Root scene component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation")
	USceneComponent* RootScene;

	// Grid of small cube boxes (like LEGO) for proper rotated nav blocking
	UPROPERTY(VisibleAnywhere, Category = "Navigation")
	TArray<UBoxComponent*> BlockingCubes;

	// Size of each cube in the grid (smaller = more precise but more components)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	float CubeSize = 50.0f;

	// Height offset - how much to extend the blocker upward from the roof
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	float HeightOffset = 0.0f;

	// How far down to extend the blocker below the house base (to handle tilted houses on slopes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	float DownwardExtension = 0.0f;

	// Extra padding around the blocker bounds (XY)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	float BoundsPadding = 0.0f;

	// Enable debug visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Debug")
	bool bShowDebugVisualization = false;

	// Color for debug visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Debug")
	FColor DebugColor = FColor::Red;

	// Number of cubes to process per frame (higher = faster but more stutter)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Performance")
	int32 CubesPerFrame = 500;

	// Random delay range before starting cube processing (spreads load across frames)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Performance")
	float MaxStartDelay = 2.0f;

	// Skip collision testing and just fill the bounding box (faster but less precise)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Performance")
	bool bSkipCollisionTest = false;

	// Whether this blocker should be processed (set during spawn)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation|Performance")
	bool bShouldProcess = true;

	// Set the box extent to match the house bounds
	// @param Extent - The mesh extent (half-size)
	// @param MeshCenterOffset - The center offset of the mesh in local space
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void SetBlockingExtent(const FVector& Extent, const FVector& MeshCenterOffset = FVector::ZeroVector);

	// Set the blocking cubes based on collision with a static mesh's geometry
	// Creates all cubes first, then removes ones that don't intersect the mesh
	// @param Extent - The mesh extent (half-size)
	// @param Mesh - The static mesh to test collision against
	// @param MeshScale - Scale applied to the mesh
	// @param MeshWorldLocation - The world location where the mesh is actually placed (pivot point)
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void SetBlockingExtentFromMesh(const FVector& Extent, UStaticMesh* Mesh, float MeshScale, const FVector& MeshWorldLocation);

	// Set the height offset
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void SetHeightOffset(float NewHeightOffset);

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	// Draw debug visualization
	void DrawDebugVisualization();
	
	// Create and configure a cube box component for nav blocking
	UBoxComponent* CreateBlockingCube(const FVector& LocalPosition, float HalfSize);

	// Async processing members
	void ProcessCollisionScanBatch();
	void ProcessSimpleCubeBatch();  // For bounding box fill without collision testing
	void FinishCubeProcessing();
	
	UPROPERTY()
	UStaticMeshComponent* TempMeshComponent = nullptr;
	
	// For collision scanning pass - positions to check
	TArray<FVector> PendingCubePositions;
	TArray<UBoxComponent*> CubesToCheck;
	int32 ProcessedCubeIndex = 0;
	int32 TotalCubesToProcess = 0;
	float StoredHalfCube = 0.0f;
	float StoredTraceDistance = 0.0f;
	FTransform StoredActorTransform;
	FTimerHandle CubeProcessingTimerHandle;
	FTimerHandle DelayedStartTimerHandle;
	bool bIsProcessingCubes = false;
	bool bPendingDelayedStart = false;

	// Stored parameters for delayed start
	FVector DelayedExtent;
	UStaticMesh* DelayedMesh = nullptr;
	float DelayedMeshScale = 1.0f;
	FVector DelayedMeshWorldLocation;  // Actual world location where the mesh is placed

	// Actually start the mesh-based blocking (called after delay)
	void StartBlockingExtentFromMesh();

	// Test if a world position is inside the mesh using ray casting
	bool IsPointInsideMesh(const FVector& WorldPos) const;
};

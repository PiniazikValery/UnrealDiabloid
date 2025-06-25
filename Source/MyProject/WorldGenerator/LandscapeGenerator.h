// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "LandscapeGenerator.generated.h"

struct FQueuedTileData
{
	TArray<FVector>			 Vertices;
	TArray<FVector2D>		 UVs;
	TArray<int32>			 Triangles;
	TArray<FVector>			 Normals;
	TArray<FProcMeshTangent> Tangents;
	FIntPoint				 Tile;

	explicit FQueuedTileData(const FIntPoint InTile)
		: Tile(InTile)
	{
		Vertices.Empty();
		UVs.Empty();
		Triangles.Empty();
		Normals.Empty();
		Tangents.Empty();
	}
};

USTRUCT(BlueprintType)
struct FTileIndicesRange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 XStart;

	UPROPERTY(BlueprintReadWrite)
	int32 XEnd;

	UPROPERTY(BlueprintReadWrite)
	int32 YStart;

	UPROPERTY(BlueprintReadWrite)
	int32 YEnd;

	FTileIndicesRange();

	FTileIndicesRange(int32 InXStart, int32 InXEnd, int32 InYStart, int32 InYEnd);
};

UCLASS()
class MYPROJECT_API ALandscapeGenerator : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALandscapeGenerator();

	UPROPERTY(EditAnywhere, Category = "Navigation")
	TSubclassOf<class UNavArea> NavAreaClass;

	UPROPERTY(EditAnywhere, BlueprintReadonly)
	int XVertexCount = 15;

	UPROPERTY(EditAnywhere, BlueprintReadonly)
	int YVertexCount = 15;

	UPROPERTY(EditAnywhere, BlueprintReadonly)
	int CellSize = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadonly)
	int NumOfSectionsX = 4;

	UPROPERTY(EditAnywhere, BlueprintReadonly)
	int NumOfSectionsY = 4;

	UPROPERTY(EditAnywhere, BlueprintReadonly)
	int MeshSectionIndex = 0;

	UPROPERTY(BlueprintReadonly)
	UProceduralMeshComponent* TerrainMesh;

	UPROPERTY(EditAnywhere, BlueprintReadonly)
	UMaterialInterface* TerrainMaterial = nullptr;
	UPROPERTY(BlueprintReadonly)
	bool DrawingTiles = false;
	UPROPERTY(BlueprintReadonly)
	bool GeneratingTerrains = false;
	UPROPERTY(BlueprintReadonly)
	bool RemovingTiles = false;
	UPROPERTY(BlueprintReadonly)
	bool GeneratingNavMeshBounds = false;
	UPROPERTY(BlueprintReadonly)
	bool RemovingNavMeshBounds = false;
	int	 SectionIndexX = 0;
	int	 SectionIndexY = 0;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	void								   GenerateTerrain(const FIntPoint TileKey);
	void								   GenerateTerrainsAsync();
	void								   DrawTile(FQueuedTileData QueuedTile);
	void								   DrawTilesAsync();
	void								   RemoveDistantTiles();
	void								   RemoveDistantTilesAsync();
	void								   RemoveDistantNavMeshBounds();
	TArray<FVector>						   GenerateVerticesByTileKey(const FIntPoint TileKey);
	void								   GenerateNavMeshBoundsNearPlayer();
	void								   GenerateNavMeshBoundsNearPlayerAsync();
	TArray<FQueuedTileData>				   QueuedTiles;
	TMap<FIntPoint, int32>				   ProcessedTiles;
	TMap<FIntPoint, ANavMeshBoundsVolume*> ProcessedNavMeshBounds;
	float								   GetHeight(const FVector2D Location);
	float								   PerlinNoiseExtended(const FVector2D Location, const float Scale, const float Amplitude, const FVector2D offset);
	UE::Math::TVector2<float>			   GetPlayerPositionIndex();
	FTileIndicesRange					   GetTilesIndicesAroundPlayer();
	FVector								   GetTileCenter(const FIntPoint TileKey);
};

class FIntPointPacker
{
public:
	/** Packs an FIntPoint into a single int32 */
	static int32 Pack(const FIntPoint& Point);
};

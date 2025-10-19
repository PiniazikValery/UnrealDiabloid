// Fill out your copyright notice in the Description page of Project Settings.

#include "LandscapeGenerator.h"
#include "KismetProceduralMeshLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Components/BrushComponent.h"
#include "NavAreas/NavArea_Null.h"

// Sets default values
ALandscapeGenerator::ALandscapeGenerator()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create an explicit root so the engine doesn't auto-pick one.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	TerrainMesh->SetupAttachment(RootComponent);
	TerrainMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/Game/LevelPrototyping/Materials/M_Landscape.M_Landscape'"));
	bReplicates = true;
}

// Called when the game starts or when spawned
void ALandscapeGenerator::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ALandscapeGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (GFrameCounter % 2 == 0)
	{
		if (!GeneratingTerrains)
		{
			GenerateTerrainsAsync();
		}
	}
	else
	{
		if (GFrameCounter % 5 == 0)
		{
			RemoveDistantTilesAsync();
			/*RemoveDistantNavMeshBounds();*/
		}
		else if (!DrawingTiles)
		{
			DrawTilesAsync();
			FTimerHandle DelayTimerHandle;
			UWorld* World = GetWorld();
			if (World)
			{
				World->GetTimerManager().SetTimer(
					DelayTimerHandle,
					this,
					&ALandscapeGenerator::GenerateNavMeshBoundsNearPlayerAsync,
					1.0f, // delay in seconds
					false // don't loop
				);
			}
			//GenerateNavMeshBoundsNearPlayerAsync();
		}
	}
}

void ALandscapeGenerator::GenerateTerrain(const FIntPoint TileKey)
{
	FVector Offset = FVector(TileKey.X * (XVertexCount - 1), TileKey.Y * (YVertexCount - 1), 0.f) * CellSize;

	TArray<FVector> Vertices = GenerateVerticesByTileKey(TileKey);

	TArray<FVector2D> UVs;
	FVector2D		  UV;

	TArray<int32>			 Triangles;
	TArray<FVector>			 Normals;
	TArray<FProcMeshTangent> Tangents;

	// Vertices and UVs
	for (int32 iVY = -1; iVY <= YVertexCount; iVY++)
	{
		for (int32 iVX = -1; iVX <= XVertexCount; iVX++)
		{
			// UV
			UV.X = (iVX + (TileKey.X * (XVertexCount - 1))) * CellSize / 100;
			UV.Y = (iVY + (TileKey.Y * (YVertexCount - 1))) * CellSize / 100;
			UVs.Add(UV);
		}
	}

	// Triangles
	for (int32 iTY = 0; iTY <= YVertexCount; iTY++)
	{
		for (int32 iTX = 0; iTX <= XVertexCount; iTX++)
		{
			Triangles.Add(iTX + iTY * (XVertexCount + 2));
			Triangles.Add(iTX + (iTY + 1) * (XVertexCount + 2));
			Triangles.Add(iTX + iTY * (XVertexCount + 2) + 1);

			Triangles.Add(iTX + (iTY + 1) * (XVertexCount + 2));
			Triangles.Add(iTX + (iTY + 1) * (XVertexCount + 2) + 1);
			Triangles.Add(iTX + iTY * (XVertexCount + 2) + 1);
		}
	}

	int VertexIndex = 0;

	// calculate normals
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

	FQueuedTileData NewSubMesh(TileKey);

	// Subset vertices and UVs
	for (int32 iVY = -1; iVY <= YVertexCount; iVY++)
	{
		for (int32 iVX = -1; iVX <= XVertexCount; iVX++)
		{
			if (-1 < iVY && iVY < YVertexCount && -1 < iVX && iVX < XVertexCount)
			{
				NewSubMesh.Vertices.Add(Vertices[VertexIndex]);
				NewSubMesh.UVs.Add(UVs[VertexIndex]);
				NewSubMesh.Normals.Add(Normals[VertexIndex]);
				NewSubMesh.Tangents.Add(Tangents[VertexIndex]);
			}
			VertexIndex++;
		}
	}

	// Subset triangles
	if (NewSubMesh.Triangles.Num() == 0)
	{
		for (int32 iTY = 0; iTY <= YVertexCount - 2; iTY++)
		{
			for (int32 iTX = 0; iTX <= XVertexCount - 2; iTX++)
			{
				NewSubMesh.Triangles.Add(iTX + iTY * XVertexCount);
				NewSubMesh.Triangles.Add(iTX + iTY * XVertexCount + XVertexCount);
				NewSubMesh.Triangles.Add(iTX + iTY * XVertexCount + 1);

				NewSubMesh.Triangles.Add(iTX + iTY * XVertexCount + XVertexCount);
				NewSubMesh.Triangles.Add(iTX + iTY * XVertexCount + XVertexCount + 1);
				NewSubMesh.Triangles.Add(iTX + iTY * XVertexCount + 1);
			}
		}
	}

	QueuedTiles.Add(NewSubMesh);
}

void ALandscapeGenerator::GenerateTerrainsAsync()
{
	GeneratingTerrains = true;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]() {
		FTileIndicesRange TilesIndicesAroundPlayer = GetTilesIndicesAroundPlayer();
		bool			  breakAll = false;

		// Calculate the center of the tile range
		const int32 CenterX = (TilesIndicesAroundPlayer.XStart + TilesIndicesAroundPlayer.XEnd) / 2;
		const int32 CenterY = (TilesIndicesAroundPlayer.YStart + TilesIndicesAroundPlayer.YEnd) / 2;

		TArray<FIntPoint> TileKeys;

		// Collect all tile coordinates
		for (int32 y = TilesIndicesAroundPlayer.YStart; y <= TilesIndicesAroundPlayer.YEnd; y++)
		{
			for (int32 x = TilesIndicesAroundPlayer.XStart; x <= TilesIndicesAroundPlayer.XEnd; x++)
			{
				TileKeys.Add(FIntPoint(x, y));
			}
		}

		// Sort tiles by Manhattan distance from the center
		TileKeys.Sort([CenterX, CenterY](const FIntPoint& A, const FIntPoint& B) {
			int32 DistA = FMath::Abs(A.X - CenterX) + FMath::Abs(A.Y - CenterY); // Manhattan Distance
			int32 DistB = FMath::Abs(B.X - CenterX) + FMath::Abs(B.Y - CenterY);
			return DistA < DistB;
		});

		// Process the closest tile first
		for (const FIntPoint& TileKey : TileKeys)
		{
			if (!ProcessedTiles.Contains(TileKey))
			{
				ProcessedTiles.Add(TileKey, -1);
				GenerateTerrain(TileKey);
			}
		}
		GeneratingTerrains = false;
	});
}

void ALandscapeGenerator::DrawTile(FQueuedTileData QueuedTile)
{
	TerrainMesh->bUseAsyncCooking = true;
	int32* FoundTile = ProcessedTiles.Find(QueuedTile.Tile);
	if (FoundTile && *FoundTile == -1)
	{
		*FoundTile = MeshSectionIndex;
	}
	TerrainMesh->CreateMeshSection(MeshSectionIndex, QueuedTile.Vertices, QueuedTile.Triangles, QueuedTile.Normals, QueuedTile.UVs, TArray<FColor>(), QueuedTile.Tangents, true);
	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(MeshSectionIndex, TerrainMaterial);
	}
	FNavigationSystem::UpdateComponentData(*TerrainMesh);
	TerrainMesh->SetCanEverAffectNavigation(true);
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerrainMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MeshSectionIndex++;
}

void ALandscapeGenerator::DrawTilesAsync()
{
	DrawingTiles = true;
	AsyncTask(ENamedThreads::GameThread, [this]() {
		for (int32 i = 0; i < QueuedTiles.Num(); i++)
		{
			DrawTile(QueuedTiles[i]);
		}
		QueuedTiles.Empty();
		DrawingTiles = false;
	});
}

void ALandscapeGenerator::RemoveDistantTiles()
{
	// Add null checks to prevent crashes
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("ALandscapeGenerator::RemoveDistantTiles - World is null"));
		return;
	}
	
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("ALandscapeGenerator::RemoveDistantTiles - PlayerController is null"));
		return;
	}
	
	APawn* PlayerPawn = PlayerController->GetPawn();
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ALandscapeGenerator::RemoveDistantTiles - PlayerPawn is null"));
		return;
	}
	
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	PlayerLocation.Z = 0;
	float MaxDistance = (CellSize * NumOfSectionsX * XVertexCount);

	TMap<FIntPoint, int32> TilesToRemove;

	for (const TPair<FIntPoint, int32>& Tail : ProcessedTiles)
	{
		FVector TileCenter = GetTileCenter(Tail.Key);
		TileCenter.Z = 0;

		float Distance = FVector::Dist(PlayerLocation, TileCenter);

		if (Distance > MaxDistance)
		{
			TilesToRemove.Add(Tail.Key, Tail.Value);
		}
	}

	for (const TPair<FIntPoint, int32>& Tail : TilesToRemove)
	{
		TerrainMesh->ClearMeshSection(Tail.Value);
		ProcessedTiles.Remove(Tail.Key);
	}
}

void ALandscapeGenerator::RemoveDistantTilesAsync()
{
	if (!RemovingTiles)
	{
		RemovingTiles = true;
		AsyncTask(ENamedThreads::GameThread, [this]() {
			RemoveDistantTiles();
			RemovingTiles = false;
		});
	}
}

void ALandscapeGenerator::RemoveDistantNavMeshBounds()
{
	TMap<FIntPoint, ANavMeshBoundsVolume*> NavMeshBoundsToRemove;

	// Identify nav mesh bounds volumes whose tiles are no longer present
	for (const TPair<FIntPoint, ANavMeshBoundsVolume*>& Entry : ProcessedNavMeshBounds)
	{
		if (!ProcessedTiles.Contains(Entry.Key))
		{
			NavMeshBoundsToRemove.Add(Entry.Key, Entry.Value);
		}
	}

	// Destroy and remove them
	for (const TPair<FIntPoint, ANavMeshBoundsVolume*>& Entry : NavMeshBoundsToRemove)
	{
		if (Entry.Value)
		{
			Entry.Value->Destroy();
		}
		ProcessedNavMeshBounds.Remove(Entry.Key);
	}
}

TArray<FVector> ALandscapeGenerator::GenerateVerticesByTileKey(const FIntPoint TileKey)
{
	FVector			Offset = FVector(TileKey.X * (XVertexCount - 1), TileKey.Y * (YVertexCount - 1), 0.f) * CellSize;
	TArray<FVector> Vertices;
	FVector			Vertex;
	for (int32 iVY = -1; iVY <= YVertexCount; iVY++)
	{
		for (int32 iVX = -1; iVX <= XVertexCount; iVX++)
		{
			// Vertex calculation
			Vertex.X = iVX * CellSize + Offset.X;
			Vertex.Y = iVY * CellSize + Offset.Y;
			Vertex.Z = GetHeight(FVector2D(Vertex.X, Vertex.Y));

			Vertices.Add(Vertex);
		}
	}
	return Vertices;
}

void ALandscapeGenerator::GenerateNavMeshBoundsNearPlayer()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	// Step 1: Find out if we need to generate anything
	TArray<FIntPoint> TilesNeedingNavMesh;
	for (const TPair<FIntPoint, int32>& Tile : ProcessedTiles)
	{
		if (!ProcessedNavMeshBounds.Contains(Tile.Key))
		{
			TilesNeedingNavMesh.Add(Tile.Key);
		}
	}

	// Step 2: If no new tiles need nav mesh bounds, skip everything
	if (TilesNeedingNavMesh.Num() == 0)
		return;

	// Step 3: Clean up old nav meshes for unloaded tiles
	RemoveDistantNavMeshBounds();

	// Step 4: Generate nav mesh bounds for required tiles
	for (const FIntPoint& TileKey : TilesNeedingNavMesh)
	{
		World->GetTimerManager().SetTimerForNextTick([this, TileKey, World]() {
			TArray<FVector> RawVertices = GenerateVerticesByTileKey(TileKey);

			// Use all vertices for nav bounds
			TArray<FVector> WorldVertices;
			WorldVertices.Reserve(RawVertices.Num());
			for (const FVector& LocalVertex : RawVertices)
			{
				WorldVertices.Add(GetActorTransform().TransformPosition(LocalVertex));
			}

			FBox	TileBounds(WorldVertices);
			FVector BoundsExtent = TileBounds.GetExtent();

			UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			if (!NavSys)
				return;

			ANavMeshBoundsVolume* BoundsVolume = World->SpawnActor<ANavMeshBoundsVolume>();
			if (!BoundsVolume)
				return;

			auto* NavMeshBrush = BoundsVolume->GetBrushComponent();
			NavMeshBrush->SetMobility(EComponentMobility::Movable);
			BoundsVolume->SetActorLocation(TileBounds.GetCenter());
			NavMeshBrush->Bounds.BoxExtent = BoundsExtent;

			NavSys->OnNavigationBoundsUpdated(BoundsVolume);
			NavSys->OnComponentRegistered(TerrainMesh);

			ProcessedNavMeshBounds.Add(TileKey, BoundsVolume);
		});
	}
}

void ALandscapeGenerator::GenerateNavMeshBoundsNearPlayerAsync()
{
	if (!GeneratingNavMeshBounds && !RemovingNavMeshBounds)
	{
		GeneratingNavMeshBounds = true;
		AsyncTask(ENamedThreads::GameThread, [this]() {
			GenerateNavMeshBoundsNearPlayer();
			GeneratingNavMeshBounds = false;
		});
	}
}

float ALandscapeGenerator::GetHeight(FVector2D Location)
{
	return PerlinNoiseExtended(Location, .00001, 20000, FVector2D(.1f));
}

float ALandscapeGenerator::PerlinNoiseExtended(const FVector2D Location, const float Scale, const float Amplitude, const FVector2D offset)
{
	return FMath::PerlinNoise2D(Location * Scale + FVector2D(1.f, 1.f) + offset) * Amplitude;
}

UE::Math::TVector2<float> ALandscapeGenerator::GetPlayerPositionIndex()
{

	UE::Math::TVector2<float> Result;
	ACharacter*				  PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (PlayerCharacter)
	{
		FVector Location = PlayerCharacter->GetActorLocation();
		Result.X = FMath::TruncToFloat(Location.X / (CellSize * (XVertexCount - 1)));
		Result.Y = FMath::TruncToFloat(Location.Y / (CellSize * (YVertexCount - 1)));
	}
	return Result;
}

FTileIndicesRange ALandscapeGenerator::GetTilesIndicesAroundPlayer()
{
	FTileIndicesRange		  Result;
	UE::Math::TVector2<float> PlayerPositionIndex = GetPlayerPositionIndex();
	Result.XStart = PlayerPositionIndex.X - (NumOfSectionsX / 2);
	Result.XEnd = NumOfSectionsX + Result.XStart - 1;
	Result.YStart = PlayerPositionIndex.Y - (NumOfSectionsY / 2);
	Result.YEnd = NumOfSectionsY + Result.YStart - 1;
	return Result;
}

FVector ALandscapeGenerator::GetTileCenter(const FIntPoint TileKey)
{
	FVector Center = FVector(
		(TileKey.X * (XVertexCount - 1) + (XVertexCount - 1) / 2.0f) * CellSize,
		(TileKey.Y * (YVertexCount - 1) + (YVertexCount - 1) / 2.0f) * CellSize,
		GetHeight(FVector2D(
			(TileKey.X * (XVertexCount - 1) + (XVertexCount - 1) / 2.0f) * CellSize,
			(TileKey.Y * (YVertexCount - 1) + (YVertexCount - 1) / 2.0f) * CellSize)));

	return Center;
}

FTileIndicesRange::FTileIndicesRange()
	: XStart(0.0), XEnd(0.0), YStart(0.0), YEnd(0.0)
{
}

FTileIndicesRange::FTileIndicesRange(int32 InXStart, int32 InXEnd, int32 InYStart, int32 InYEnd)
	: XStart(InXStart), XEnd(InXEnd), YStart(InYStart), YEnd(InYEnd)
{
}

int32 FIntPointPacker::Pack(const FIntPoint& Point)
{
	int32 X = Point.X + 32768; // Convert to non-negative
	int32 Y = Point.Y + 32768;

	int32 Hash = (X * 73856093) ^ (Y * 19349663); // Large prime multipliers
	return Hash & 0x7FFFFFFF;					  // Ensure positive int32
}

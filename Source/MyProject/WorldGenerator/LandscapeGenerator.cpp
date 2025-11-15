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
#include "DrawDebugHelpers.h"
#include "PhysicsEngine/BodySetup.h"

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
	
	// Create instanced static mesh component for trees
	TreeInstancedMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeInstancedMesh"));
	TreeInstancedMesh->SetupAttachment(RootComponent);
	TreeInstancedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TreeInstancedMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TreeInstancedMesh->SetCastShadow(true);
	
	// Enable navigation for trees
	TreeInstancedMesh->SetCanEverAffectNavigation(true);
	TreeInstancedMesh->bFillCollisionUnderneathForNavmesh = true;
	
	// Load the tree mesh
	TreeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Game/Models/Trees/scene/StaticMeshes/scene.scene'"));
	if (TreeMesh)
	{
		TreeInstancedMesh->SetStaticMesh(TreeMesh);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load tree mesh!"));
	}
	
	// Create instanced static mesh component for cacti (desert biome)
	CactusInstancedMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CactusInstancedMesh"));
	CactusInstancedMesh->SetupAttachment(RootComponent);
	CactusInstancedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CactusInstancedMesh->SetCollisionResponseToAllChannels(ECR_Block);
	CactusInstancedMesh->SetCastShadow(true);
	
	// Enable navigation for cacti
	CactusInstancedMesh->SetCanEverAffectNavigation(true);
	CactusInstancedMesh->bFillCollisionUnderneathForNavmesh = true;
	
	// Load the cactus mesh
	CactusMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Game/Models/Cactuses/scene/StaticMeshes/SM_MERGED_Cactus.SM_MERGED_Cactus'"));
	if (CactusMesh)
	{
		CactusInstancedMesh->SetStaticMesh(CactusMesh);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load cactus mesh!"));
	}
	
	// Load multiple rock meshes (5 types)
	RockMeshes.Add(LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Game/Models/Rocks/scene/StaticMeshes/MRock1_MRock01_0.MRock1_MRock01_0'")));
	RockMeshes.Add(LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Game/Models/Rocks/scene/StaticMeshes/MRock2_MRock02_0.MRock2_MRock02_0'")));
	RockMeshes.Add(LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Game/Models/Rocks/scene/StaticMeshes/MRock3_MRock03_0.MRock3_MRock03_0'")));
	RockMeshes.Add(LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Game/Models/Rocks/scene/StaticMeshes/MRock4_MRock04_0.MRock4_MRock04_0'")));
	RockMeshes.Add(LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Game/Models/Rocks/scene/StaticMeshes/MRock5_MRock05_0.MRock5_MRock05_0'")));
	
	// Create one instanced static mesh component for each rock type
	for (int32 i = 0; i < RockMeshes.Num(); i++)
	{
		FString ComponentName = FString::Printf(TEXT("RockInstancedMesh_%d"), i);
		UInstancedStaticMeshComponent* RockComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*ComponentName);
		RockComponent->SetupAttachment(RootComponent);
		RockComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		RockComponent->SetCollisionResponseToAllChannels(ECR_Block); // Block all channels including Pawn
		RockComponent->SetCastShadow(true);
		
		// Use simple sphere collision instead of complex mesh collision
		RockComponent->SetCollisionObjectType(ECC_WorldStatic);
		RockComponent->BodyInstance.SetCollisionProfileName(TEXT("BlockAll"));
		
		// Enable navigation - rocks will affect nav mesh
		RockComponent->SetCanEverAffectNavigation(true);
		RockComponent->bFillCollisionUnderneathForNavmesh = true;
		
		if (RockMeshes[i])
		{
			RockComponent->SetStaticMesh(RockMeshes[i]);
			
			// Override collision to use simple collision (sphere/box) instead of complex mesh
			// This will use a smaller, more reasonable collision bound
			if (RockMeshes[i]->GetBodySetup())
			{
				RockMeshes[i]->GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseSimpleAsComplex;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load rock mesh %d!"), i);
		}
		
		RockInstancedMeshes.Add(RockComponent);
	}
	
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
	
	// Spawn vegetation - they will check individual position heights
	// SpawnCactiOnTile now spawns both cacti AND rocks (rocks replace some cacti)
	SpawnCactiOnTile(QueuedTile.Tile);   // Cacti and rocks (desert)
	SpawnTreesOnTile(QueuedTile.Tile);   // Trees (grassland)
	
	// Update navigation for all vegetation including rocks
	for (UInstancedStaticMeshComponent* RockComponent : RockInstancedMeshes)
	{
		if (RockComponent)
		{
			FNavigationSystem::UpdateComponentData(*RockComponent);
		}
	}
	FNavigationSystem::UpdateComponentData(*CactusInstancedMesh);
	FNavigationSystem::UpdateComponentData(*TreeInstancedMesh);
	
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
		// Remove all vegetation (RemoveCactiOnTile handles both cacti and rocks)
		RemoveTreesOnTile(Tail.Key);
		RemoveCactiOnTile(Tail.Key);  // This also removes rocks
		
		// Then remove terrain
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

	// Get player location
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
		return;
	
	APawn* PlayerPawn = PlayerController->GetPawn();
	if (!PlayerPawn)
		return;
	
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	PlayerLocation.Z = 0;
	
	// Define nav mesh generation radius (in tiles around player)
	// Adjust this value to control how many tiles around player get nav mesh
	int32 NavMeshRadiusInTiles = 1; // Only generate nav mesh for 1 tiles around player
	float NavMeshMaxDistance = NavMeshRadiusInTiles * (CellSize * XVertexCount);

	// Step 1: Find tiles near player that need nav mesh
	TArray<FIntPoint> TilesNeedingNavMesh;
	for (const TPair<FIntPoint, int32>& Tile : ProcessedTiles)
	{
		FVector TileCenter = GetTileCenter(Tile.Key);
		TileCenter.Z = 0;
		
		float Distance = FVector::Dist(PlayerLocation, TileCenter);
		
		// Only generate nav mesh for tiles within radius of player
		if (Distance <= NavMeshMaxDistance)
		{
			if (!ProcessedNavMeshBounds.Contains(Tile.Key))
			{
				TilesNeedingNavMesh.Add(Tile.Key);
			}
		}
	}

	// Step 2: If no new tiles need nav mesh bounds, skip everything
	if (TilesNeedingNavMesh.Num() == 0)
		return;

	// Step 3: Clean up old nav meshes for tiles far from player or unloaded tiles
	TArray<FIntPoint> NavMeshBoundsToRemove;
	for (const TPair<FIntPoint, ANavMeshBoundsVolume*>& Entry : ProcessedNavMeshBounds)
	{
		// Remove if tile no longer exists OR if it's too far from player
		if (!ProcessedTiles.Contains(Entry.Key))
		{
			NavMeshBoundsToRemove.Add(Entry.Key);
		}
		else
		{
			FVector TileCenter = GetTileCenter(Entry.Key);
			TileCenter.Z = 0;
			float Distance = FVector::Dist(PlayerLocation, TileCenter);
			
			if (Distance > NavMeshMaxDistance)
			{
				NavMeshBoundsToRemove.Add(Entry.Key);
			}
		}
	}
	
	// Destroy nav mesh bounds that are too far
	for (const FIntPoint& TileKey : NavMeshBoundsToRemove)
	{
		if (ProcessedNavMeshBounds.Contains(TileKey))
		{
			ANavMeshBoundsVolume* BoundsVolume = ProcessedNavMeshBounds[TileKey];
			if (BoundsVolume)
			{
				BoundsVolume->Destroy();
			}
			ProcessedNavMeshBounds.Remove(TileKey);
		}
	}

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

float ALandscapeGenerator::TreePerlinNoise(const FVector2D Location, const float Scale, const FVector2D offset)
{
	// Use different seed offset for trees to make it independent from terrain
	FVector2D TreeOffset = FVector2D(9876.543f, 6543.210f) + offset;
	return FMath::PerlinNoise2D(Location * Scale + TreeOffset);
}

// Fractal Brownian Motion for better noise
float ALandscapeGenerator::TreeFBMNoise(const FVector2D Location, int32 Octaves, float Persistence, float Lacunarity)
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float Frequency = 0.001f; // Base frequency for world-space coordinates
	float MaxValue = 0.0f;
	
	for (int32 i = 0; i < Octaves; i++)
	{
		Total += TreePerlinNoise(Location, Frequency, FVector2D(i * 100.0f, i * 150.0f)) * Amplitude;
		MaxValue += Amplitude;
		Amplitude *= Persistence;
		Frequency *= Lacunarity;
	}
	
	// Normalize to [0, 1]
	return (Total / MaxValue + 1.0f) * 0.5f;
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

// Tree spawning implementation
void ALandscapeGenerator::SpawnTreesOnTile(const FIntPoint& TileKey)
{
	TArray<FVector> TreePositions;
	TArray<FTransform> TreeTransforms;
	
	// Calculate tile boundaries
	float TileMinX = TileKey.X * (XVertexCount - 1) * CellSize;
	float TileMaxX = (TileKey.X + 1) * (XVertexCount - 1) * CellSize;
	float TileMinY = TileKey.Y * (YVertexCount - 1) * CellSize;
	float TileMaxY = (TileKey.Y + 1) * (YVertexCount - 1) * CellSize;
	
	// Create a world-space grid that's completely independent of tiles
	// Use a fixed grid cell size based on tree spacing
	float GridCellSize = MinTreeDistance * 0.5f; // Smaller cells for more coverage attempts
	
	// Calculate which grid cells overlap this tile (with padding for neighbor checking)
	float SearchPadding = MinTreeDistance * 2.0f;
	int32 GridStartX = FMath::FloorToInt((TileMinX - SearchPadding) / GridCellSize);
	int32 GridEndX = FMath::CeilToInt((TileMaxX + SearchPadding) / GridCellSize);
	int32 GridStartY = FMath::FloorToInt((TileMinY - SearchPadding) / GridCellSize);
	int32 GridEndY = FMath::CeilToInt((TileMaxY + SearchPadding) / GridCellSize);
	
	// Iterate through world-space grid cells (not tile-based!)
	for (int32 GridY = GridStartY; GridY <= GridEndY; GridY++)
	{
		for (int32 GridX = GridStartX; GridX <= GridEndX; GridX++)
		{
			// Use grid coordinates as seed for deterministic randomness
			// This ensures the same tree appears in the same world position regardless of which tile generates it
			FVector2D GridCoord(GridX * 1000.0f, GridY * 1000.0f);
			
			// Generate position within this grid cell using noise
			float PosNoiseX = TreePerlinNoise(GridCoord, 0.01f, FVector2D(1234.5f, 6789.1f));
			float PosNoiseY = TreePerlinNoise(GridCoord, 0.01f, FVector2D(9876.5f, 4321.1f));
			
			float NormalizedX = (PosNoiseX + 1.0f) * 0.5f;
			float NormalizedY = (PosNoiseY + 1.0f) * 0.5f;
			
			// Calculate world position within the grid cell
			float WorldX = GridX * GridCellSize + NormalizedX * GridCellSize;
			float WorldY = GridY * GridCellSize + NormalizedY * GridCellSize;
			
			// Use world position for all noise checks - completely tile-independent
			FVector2D WorldPos(WorldX, WorldY);
			float PlacementNoise = TreeFBMNoise(WorldPos, 4, 0.5f, 2.0f);
			float DetailNoise = TreeFBMNoise(WorldPos, 2, 0.3f, 3.0f);
			float CullingNoise = TreePerlinNoise(WorldPos, 0.0005f, FVector2D(5000.0f, 7000.0f));
			float CullingNoise2 = TreePerlinNoise(WorldPos, 0.0008f, FVector2D(8000.0f, 12000.0f));
			
			// Add third culling layer to significantly reduce overall tree density
			float DensityReduction = TreePerlinNoise(WorldPos, 0.0003f, FVector2D(15000.0f, 20000.0f));
			
			float CombinedValue = PlacementNoise * 0.7f + DetailNoise * 0.3f;
			
			// Apply all noise filters
			if (CombinedValue < 0.25f) continue;
			if (CullingNoise < -0.3f) continue;
			if (CullingNoise2 < -0.2f) continue;
			
			// Use very large-scale noise to remove many trees, creating sparser forest
			// Higher threshold = fewer trees (removes more)
			if (DensityReduction < 0.4f) continue;
			
			FVector TreePosition(WorldX, WorldY, 0);
			
			// Only process trees that are actually in this tile's boundaries
			bool IsInTile = (WorldX >= TileMinX && WorldX < TileMaxX && 
			                 WorldY >= TileMinY && WorldY < TileMaxY);
			
			// Check distance to trees in this tile
			if (!IsValidTreePosition(TreePosition, TreePositions))
			{
				continue;
			}
			
			// Check distance to trees in neighboring tiles
			bool ValidAcrossTiles = true;
			for (int32 dx = -1; dx <= 1; dx++)
			{
				for (int32 dy = -1; dy <= 1; dy++)
				{
					if (dx == 0 && dy == 0) continue;
					
					FIntPoint NeighborTile(TileKey.X + dx, TileKey.Y + dy);
					if (TileTreeTransforms.Contains(NeighborTile))
					{
						const TArray<FTransform>& NeighborTrees = TileTreeTransforms[NeighborTile];
						for (const FTransform& NeighborTree : NeighborTrees)
						{
							float Distance = FVector::Dist2D(TreePosition, NeighborTree.GetLocation());
							if (Distance < MinTreeDistance)
							{
								ValidAcrossTiles = false;
								break;
							}
						}
						if (!ValidAcrossTiles) break;
					}
				}
				if (!ValidAcrossTiles) break;
			}
			
			if (!ValidAcrossTiles) continue;
			
			// Only add if actually in this tile
			if (IsInTile)
			{
				// Calculate proper height
				TreePosition.Z = GetHeight(FVector2D(TreePosition.X, TreePosition.Y));
				
				// Skip if position is below biome threshold (desert area)
				if (TreePosition.Z < BiomeHeightThreshold)
				{
					continue;
				}
				
				// Deterministic rotation and scale based on world position
				FVector2D RotScaleNoise = FVector2D(WorldX * 0.01f, WorldY * 0.01f);
				float RotationNoise = TreePerlinNoise(RotScaleNoise, 0.18f, FVector2D(7000.0f, 8000.0f));
				float ScaleNoise = TreePerlinNoise(RotScaleNoise, 0.22f, FVector2D(9000.0f, 10000.0f));
				
				float TreeRotation = (RotationNoise + 1.0f) * 0.5f * 360.0f;
				FRotator DeterministicRotation(0, TreeRotation, 0);
				
				float NormalizedScale = (ScaleNoise + 1.0f) * 0.5f;
				float TreeScale = FMath::Lerp(TreeScaleMin, TreeScaleMax, NormalizedScale);
				FVector ScaleVector(TreeScale, TreeScale, TreeScale);
				
				FTransform TreeTransform(DeterministicRotation, TreePosition, ScaleVector);
				TreeInstancedMesh->AddInstance(TreeTransform);
				
				TreePositions.Add(TreePosition);
				TreeTransforms.Add(TreeTransform);
			}
		}
	}
	
	// Store the transforms for this tile for later cleanup
	if (TreeTransforms.Num() > 0)
	{
		TileTreeTransforms.Add(TileKey, TreeTransforms);
	}
}

// Spawn cacti on desert biome tiles
void ALandscapeGenerator::SpawnCactiOnTile(const FIntPoint& TileKey)
{
	TArray<FVector> CactusPositions;
	TArray<FTransform> CactusTransforms;
	
	// Calculate tile boundaries
	float TileMinX = TileKey.X * (XVertexCount - 1) * CellSize;
	float TileMaxX = (TileKey.X + 1) * (XVertexCount - 1) * CellSize;
	float TileMinY = TileKey.Y * (YVertexCount - 1) * CellSize;
	float TileMaxY = (TileKey.Y + 1) * (YVertexCount - 1) * CellSize;
	
	// Create a world-space grid that's completely independent of tiles
	float GridCellSize = MinCactusDistance * 0.5f; // Smaller cells for more coverage attempts
	
	// Calculate which grid cells overlap this tile (with padding for neighbor checking)
	float SearchPadding = MinCactusDistance * 2.0f;
	int32 GridStartX = FMath::FloorToInt((TileMinX - SearchPadding) / GridCellSize);
	int32 GridEndX = FMath::CeilToInt((TileMaxX + SearchPadding) / GridCellSize);
	int32 GridStartY = FMath::FloorToInt((TileMinY - SearchPadding) / GridCellSize);
	int32 GridEndY = FMath::CeilToInt((TileMaxY + SearchPadding) / GridCellSize);
	
	// Iterate through world-space grid cells
	for (int32 GridY = GridStartY; GridY <= GridEndY; GridY++)
	{
		for (int32 GridX = GridStartX; GridX <= GridEndX; GridX++)
		{
			// Use grid coordinates as seed for deterministic randomness
			FVector2D GridCoord(GridX * 1000.0f, GridY * 1000.0f);
			
			// Generate position within this grid cell using noise (use different offsets than trees)
			float PosNoiseX = TreePerlinNoise(GridCoord, 0.01f, FVector2D(5678.9f, 4321.0f));
			float PosNoiseY = TreePerlinNoise(GridCoord, 0.01f, FVector2D(8765.4f, 3210.9f));
			
			float NormalizedX = (PosNoiseX + 1.0f) * 0.5f;
			float NormalizedY = (PosNoiseY + 1.0f) * 0.5f;
			
			// Calculate world position within the grid cell
			float WorldX = GridX * GridCellSize + NormalizedX * GridCellSize;
			float WorldY = GridY * GridCellSize + NormalizedY * GridCellSize;
			
			// Use world position for all noise checks
			FVector2D WorldPos(WorldX, WorldY);
			float PlacementNoise = TreeFBMNoise(WorldPos, 3, 0.4f, 2.5f);
			float DetailNoise = TreeFBMNoise(WorldPos, 2, 0.35f, 2.8f);
			float CullingNoise = TreePerlinNoise(WorldPos, 0.0006f, FVector2D(3000.0f, 4000.0f));
			float CullingNoise2 = TreePerlinNoise(WorldPos, 0.0009f, FVector2D(6000.0f, 9000.0f));
			
			// Desert density control - cacti are sparser than trees
			float DensityReduction = TreePerlinNoise(WorldPos, 0.0004f, FVector2D(12000.0f, 16000.0f));
			
			float CombinedValue = PlacementNoise * 0.6f + DetailNoise * 0.4f;
			
			// Apply all noise filters (stricter than trees for sparse desert feel)
			if (CombinedValue < 0.35f) continue;
			if (CullingNoise < -0.2f) continue;
			if (CullingNoise2 < -0.15f) continue;
			if (DensityReduction < 0.5f) continue; // Even sparser than trees
			
			FVector CactusPosition(WorldX, WorldY, 0);
			
			// Only process cacti that are actually in this tile's boundaries
			bool IsInTile = (WorldX >= TileMinX && WorldX < TileMaxX && 
			                 WorldY >= TileMinY && WorldY < TileMaxY);
			
			// Check distance to cacti in this tile
			if (!IsValidTreePosition(CactusPosition, CactusPositions))
			{
				continue;
			}
			
			// Check distance to cacti in neighboring tiles
			bool ValidAcrossTiles = true;
			for (int32 dx = -1; dx <= 1; dx++)
			{
				for (int32 dy = -1; dy <= 1; dy++)
				{
					if (dx == 0 && dy == 0) continue;
					
					FIntPoint NeighborTile(TileKey.X + dx, TileKey.Y + dy);
					if (TileCactusTransforms.Contains(NeighborTile))
					{
						const TArray<FTransform>& NeighborCacti = TileCactusTransforms[NeighborTile];
						for (const FTransform& NeighborCactus : NeighborCacti)
						{
							float Distance = FVector::Dist2D(CactusPosition, NeighborCactus.GetLocation());
							if (Distance < MinCactusDistance)
							{
								ValidAcrossTiles = false;
								break;
							}
						}
						if (!ValidAcrossTiles) break;
					}
				}
				if (!ValidAcrossTiles) break;
			}
			
			if (!ValidAcrossTiles) continue;
			
			// Only add if actually in this tile
			if (IsInTile)
			{
				// Calculate proper height
				CactusPosition.Z = GetHeight(FVector2D(CactusPosition.X, CactusPosition.Y));
				
				// Skip if position is above biome threshold (grassland area)
				if (CactusPosition.Z >= BiomeHeightThreshold)
				{
					continue;
				}
				
				// Deterministically decide whether to spawn a cactus or rock (based on world position)
				FVector2D TypeNoise = FVector2D(WorldX * 0.001f, WorldY * 0.001f);
				float TypeNoiseValue = TreePerlinNoise(TypeNoise, 0.15f, FVector2D(11000.0f, 13000.0f));
				float NormalizedTypeValue = (TypeNoiseValue + 1.0f) * 0.5f; // [0, 1]
				
				bool bSpawnRock = NormalizedTypeValue < 0.65f; // 65% chance for rocks, 35% for cacti
				
				// Deterministic rotation and scale based on world position
				FVector2D RotScaleNoise = FVector2D(WorldX * 0.01f, WorldY * 0.01f);
				float RotationNoise = TreePerlinNoise(RotScaleNoise, 0.20f, FVector2D(4000.0f, 5000.0f));
				float ScaleNoise = TreePerlinNoise(RotScaleNoise, 0.25f, FVector2D(6000.0f, 7000.0f));
				
				if (bSpawnRock)
				{
					// Spawn a rock instead of cactus
					// Select rock type deterministically
					int32 RockTypeIndex = FMath::FloorToInt(NormalizedTypeValue * 10.0f * RockMeshes.Num()) % RockMeshes.Num();
					
					float RockRotation = (RotationNoise + 1.0f) * 0.5f * 360.0f;
					FRotator DeterministicRotation(0, RockRotation, 0);
					
					float NormalizedScale = (ScaleNoise + 1.0f) * 0.5f;
					float RockScale = FMath::Lerp(RockScaleMin, RockScaleMax, NormalizedScale);
					FVector ScaleVector(RockScale, RockScale, RockScale);
					
					FTransform RockTransform(DeterministicRotation, CactusPosition, ScaleVector);
					
					// Add to appropriate rock component
					if (RockInstancedMeshes.IsValidIndex(RockTypeIndex) && RockInstancedMeshes[RockTypeIndex])
					{
						RockInstancedMeshes[RockTypeIndex]->AddInstance(RockTransform);
					}
				}
				else
				{
					// Spawn cactus
					float CactusRotation = (RotationNoise + 1.0f) * 0.5f * 360.0f;
					FRotator DeterministicRotation(0, CactusRotation, 0);
					
					float NormalizedScale = (ScaleNoise + 1.0f) * 0.5f;
					float CactusScale = FMath::Lerp(CactusScaleMin, CactusScaleMax, NormalizedScale);
					FVector ScaleVector(CactusScale, CactusScale, CactusScale);
					
					FTransform CactusTransform(DeterministicRotation, CactusPosition, ScaleVector);
					CactusInstancedMesh->AddInstance(CactusTransform);
					
					CactusTransforms.Add(CactusTransform);
				}
				
				CactusPositions.Add(CactusPosition);
			}
		}
	}
	
	// Store the transforms for this tile for later cleanup
	if (CactusTransforms.Num() > 0)
	{
		TileCactusTransforms.Add(TileKey, CactusTransforms);
	}
}

void ALandscapeGenerator::RemoveTreesOnTile(const FIntPoint& TileKey)
{
	// Check if this tile has trees
	if (!TileTreeTransforms.Contains(TileKey))
	{
		return;
	}
	
	// Remove the tile from our tracking map
	TileTreeTransforms.Remove(TileKey);
	
	// Clear all instances and rebuild from remaining tiles
	TreeInstancedMesh->ClearInstances();
	
	// Rebuild all tree instances from remaining tiles
	for (const auto& TilePair : TileTreeTransforms)
	{
		for (const FTransform& TreeTransform : TilePair.Value)
		{
			TreeInstancedMesh->AddInstance(TreeTransform);
		}
	}
	
	// Mark render state dirty to update the instanced mesh
	TreeInstancedMesh->MarkRenderStateDirty();
	
	// Update navigation after removing trees
	FNavigationSystem::UpdateComponentData(*TreeInstancedMesh);
}

void ALandscapeGenerator::RemoveCactiOnTile(const FIntPoint& TileKey)
{
	// Check if this tile has cacti
	if (!TileCactusTransforms.Contains(TileKey))
	{
		return;
	}
	
	// Remove the tile from our tracking map
	TileCactusTransforms.Remove(TileKey);
	
	// Clear all cactus instances and rebuild
	CactusInstancedMesh->ClearInstances();
	
	// Clear all rock instances too (since rocks are spawned with cacti)
	for (UInstancedStaticMeshComponent* RockComponent : RockInstancedMeshes)
	{
		if (RockComponent)
		{
			RockComponent->ClearInstances();
		}
	}
	
	// Rebuild all cactus and rock instances from remaining tiles
	for (const auto& TilePair : TileCactusTransforms)
	{
		const FIntPoint& OtherTileKey = TilePair.Key;
		
		// Recalculate what should spawn on this tile (need to replay the spawn logic)
		// We'll use the stored positions but recalculate if it should be rock or cactus
		for (const FTransform& CactusTransform : TilePair.Value)
		{
			FVector Position = CactusTransform.GetLocation();
			
			// Deterministically decide if this was a rock or cactus
			FVector2D TypeNoise = FVector2D(Position.X * 0.001f, Position.Y * 0.001f);
			float TypeNoiseValue = TreePerlinNoise(TypeNoise, 0.15f, FVector2D(11000.0f, 13000.0f));
			float NormalizedTypeValue = (TypeNoiseValue + 1.0f) * 0.5f;
			
			bool bIsRock = NormalizedTypeValue < 0.35f;
			
			if (bIsRock)
			{
				// Rebuild rock
				int32 RockTypeIndex = FMath::FloorToInt(NormalizedTypeValue * 10.0f * RockMeshes.Num()) % RockMeshes.Num();
				if (RockInstancedMeshes.IsValidIndex(RockTypeIndex) && RockInstancedMeshes[RockTypeIndex])
				{
					RockInstancedMeshes[RockTypeIndex]->AddInstance(CactusTransform);
				}
			}
			else
			{
				// Rebuild cactus
				CactusInstancedMesh->AddInstance(CactusTransform);
			}
		}
	}
	
	// Mark render state dirty
	CactusInstancedMesh->MarkRenderStateDirty();
	for (UInstancedStaticMeshComponent* RockComponent : RockInstancedMeshes)
	{
		if (RockComponent)
		{
			RockComponent->MarkRenderStateDirty();
			FNavigationSystem::UpdateComponentData(*RockComponent);
		}
	}
	
	// Update navigation after removing cacti
	FNavigationSystem::UpdateComponentData(*CactusInstancedMesh);
}

// Spawn rocks on desert biome tiles
void ALandscapeGenerator::SpawnRocksOnTile(const FIntPoint& TileKey)
{
	TArray<FVector> RockPositions;
	TArray<TPair<int32, FTransform>> RockTransformsWithType; // Store rock type index with transform
	
	// Calculate tile boundaries
	float TileMinX = TileKey.X * (XVertexCount - 1) * CellSize;
	float TileMaxX = (TileKey.X + 1) * (XVertexCount - 1) * CellSize;
	float TileMinY = TileKey.Y * (YVertexCount - 1) * CellSize;
	float TileMaxY = (TileKey.Y + 1) * (YVertexCount - 1) * CellSize;
	
	// Create a world-space grid
	float GridCellSize = MinRockDistance * 0.5f;
	
	// Calculate grid cells with padding
	float SearchPadding = MinRockDistance * 2.0f;
	int32 GridStartX = FMath::FloorToInt((TileMinX - SearchPadding) / GridCellSize);
	int32 GridEndX = FMath::CeilToInt((TileMaxX + SearchPadding) / GridCellSize);
	int32 GridStartY = FMath::FloorToInt((TileMinY - SearchPadding) / GridCellSize);
	int32 GridEndY = FMath::CeilToInt((TileMaxY + SearchPadding) / GridCellSize);
	
	// Iterate through world-space grid cells
	for (int32 GridY = GridStartY; GridY <= GridEndY; GridY++)
	{
		for (int32 GridX = GridStartX; GridX <= GridEndX; GridX++)
		{
			// Use grid coordinates for deterministic randomness
			FVector2D GridCoord(GridX * 1000.0f, GridY * 1000.0f);
			
			// Generate position within grid cell (different offsets than cacti/trees)
			float PosNoiseX = TreePerlinNoise(GridCoord, 0.01f, FVector2D(2345.6f, 5432.1f));
			float PosNoiseY = TreePerlinNoise(GridCoord, 0.01f, FVector2D(7654.3f, 2109.8f));
			
			float NormalizedX = (PosNoiseX + 1.0f) * 0.5f;
			float NormalizedY = (PosNoiseY + 1.0f) * 0.5f;
			
			// Calculate world position
			float WorldX = GridX * GridCellSize + NormalizedX * GridCellSize;
			float WorldY = GridY * GridCellSize + NormalizedY * GridCellSize;
			
			// Use world position for noise checks
			FVector2D WorldPos(WorldX, WorldY);
			float PlacementNoise = TreeFBMNoise(WorldPos, 3, 0.45f, 2.3f);
			float DetailNoise = TreeFBMNoise(WorldPos, 2, 0.3f, 2.6f);
			float CullingNoise = TreePerlinNoise(WorldPos, 0.0007f, FVector2D(2000.0f, 3500.0f));
			float CullingNoise2 = TreePerlinNoise(WorldPos, 0.001f, FVector2D(5500.0f, 8500.0f));
			
			// Density control - rocks can be fairly common
			float DensityReduction = TreePerlinNoise(WorldPos, 0.0005f, FVector2D(10000.0f, 14000.0f));
			
			float CombinedValue = PlacementNoise * 0.65f + DetailNoise * 0.35f;
			
			// Apply noise filters (less strict than trees/cacti for more rocks)
			if (CombinedValue < 0.3f) continue;
			if (CullingNoise < -0.25f) continue;
			if (CullingNoise2 < -0.2f) continue;
			if (DensityReduction < 0.45f) continue;
			
			FVector RockPosition(WorldX, WorldY, 0);
			
			// Check if in tile
			bool IsInTile = (WorldX >= TileMinX && WorldX < TileMaxX && 
			                 WorldY >= TileMinY && WorldY < TileMaxY);
			
			// Check distance to rocks in this tile
			if (!IsValidTreePosition(RockPosition, RockPositions))
			{
				continue;
			}
			
			// Check distance to rocks in neighboring tiles
			bool ValidAcrossTiles = true;
			for (int32 dx = -1; dx <= 1; dx++)
			{
				for (int32 dy = -1; dy <= 1; dy++)
				{
					if (dx == 0 && dy == 0) continue;
					
					FIntPoint NeighborTile(TileKey.X + dx, TileKey.Y + dy);
					if (TileRockTransforms.Contains(NeighborTile))
					{
						const TArray<FTransform>& NeighborRocks = TileRockTransforms[NeighborTile];
						for (const FTransform& NeighborRock : NeighborRocks)
						{
							float Distance = FVector::Dist2D(RockPosition, NeighborRock.GetLocation());
							if (Distance < MinRockDistance)
							{
								ValidAcrossTiles = false;
								break;
							}
						}
						if (!ValidAcrossTiles) break;
					}
				}
				if (!ValidAcrossTiles) break;
			}
			
			if (!ValidAcrossTiles) continue;
			
			// Only add if in tile
			if (IsInTile)
			{
				// Calculate proper height
				RockPosition.Z = GetHeight(FVector2D(RockPosition.X, RockPosition.Y));
				
				// Skip if position is above biome threshold (grassland area)
				if (RockPosition.Z >= BiomeHeightThreshold)
				{
					continue;
				}
				
				// Deterministically select rock type based on world position
				FVector2D TypeNoise = FVector2D(WorldX * 0.001f, WorldY * 0.001f);
				float TypeNoiseValue = TreePerlinNoise(TypeNoise, 0.15f, FVector2D(11000.0f, 13000.0f));
				float NormalizedTypeValue = (TypeNoiseValue + 1.0f) * 0.5f; // Convert to [0, 1]
				int32 RockTypeIndex = FMath::FloorToInt(NormalizedTypeValue * RockMeshes.Num()) % RockMeshes.Num();
				
				// Deterministic rotation and scale
				FVector2D RotScaleNoise = FVector2D(WorldX * 0.01f, WorldY * 0.01f);
				float RotationNoise = TreePerlinNoise(RotScaleNoise, 0.17f, FVector2D(3000.0f, 4500.0f));
				float ScaleNoise = TreePerlinNoise(RotScaleNoise, 0.23f, FVector2D(5500.0f, 6500.0f));
				
				// Full 3D rotation for rocks (they can be at any angle)
				float RockYaw = (RotationNoise + 1.0f) * 0.5f * 360.0f;
				FRotator DeterministicRotation(0, RockYaw, 0);
				
				float NormalizedScale = (ScaleNoise + 1.0f) * 0.5f;
				float RockScale = FMath::Lerp(RockScaleMin, RockScaleMax, NormalizedScale);
				FVector ScaleVector(RockScale, RockScale, RockScale);
				
				FTransform RockTransform(DeterministicRotation, RockPosition, ScaleVector);
				
				// Add instance to the appropriate rock mesh component
				if (RockInstancedMeshes.IsValidIndex(RockTypeIndex) && RockInstancedMeshes[RockTypeIndex])
				{
					RockInstancedMeshes[RockTypeIndex]->AddInstance(RockTransform);
				}
				
				RockPositions.Add(RockPosition);
				RockTransformsWithType.Add(TPair<int32, FTransform>(RockTypeIndex, RockTransform));
			}
		}
	}
	
	// Store transforms - we only store the transforms, not type (we can rebuild from world pos)
	if (RockTransformsWithType.Num() > 0)
	{
		TArray<FTransform> Transforms;
		for (const auto& Pair : RockTransformsWithType)
		{
			Transforms.Add(Pair.Value);
		}
		TileRockTransforms.Add(TileKey, Transforms);
	}
}

void ALandscapeGenerator::RemoveRocksOnTile(const FIntPoint& TileKey)
{
	// Check if this tile has rocks
	if (!TileRockTransforms.Contains(TileKey))
	{
		return;
	}
	
	// Remove the tile from tracking
	TileRockTransforms.Remove(TileKey);
	
	// Clear all rock instances and rebuild from remaining tiles
	for (UInstancedStaticMeshComponent* RockComponent : RockInstancedMeshes)
	{
		if (RockComponent)
		{
			RockComponent->ClearInstances();
		}
	}
	
	// Rebuild all rock instances from remaining tiles
	for (const auto& TilePair : TileRockTransforms)
	{
		for (const FTransform& RockTransform : TilePair.Value)
		{
			// Recalculate rock type from position (deterministic)
			FVector Position = RockTransform.GetLocation();
			FVector2D TypeNoise = FVector2D(Position.X * 0.001f, Position.Y * 0.001f);
			float TypeNoiseValue = TreePerlinNoise(TypeNoise, 0.15f, FVector2D(11000.0f, 13000.0f));
			float NormalizedTypeValue = (TypeNoiseValue + 1.0f) * 0.5f;
			int32 RockTypeIndex = FMath::FloorToInt(NormalizedTypeValue * RockMeshes.Num()) % RockMeshes.Num();
			
			if (RockInstancedMeshes.IsValidIndex(RockTypeIndex) && RockInstancedMeshes[RockTypeIndex])
			{
				RockInstancedMeshes[RockTypeIndex]->AddInstance(RockTransform);
			}
		}
	}
	
	// Mark render state dirty
	for (UInstancedStaticMeshComponent* RockComponent : RockInstancedMeshes)
	{
		if (RockComponent)
		{
			RockComponent->MarkRenderStateDirty();
			FNavigationSystem::UpdateComponentData(*RockComponent);
		}
	}
}

FVector ALandscapeGenerator::GetRandomPointOnTile(const FIntPoint& TileKey)
{
	// Calculate tile boundaries
	float TileMinX = TileKey.X * (XVertexCount - 1) * CellSize;
	float TileMaxX = (TileKey.X + 1) * (XVertexCount - 1) * CellSize;
	float TileMinY = TileKey.Y * (YVertexCount - 1) * CellSize;
	float TileMaxY = (TileKey.Y + 1) * (YVertexCount - 1) * CellSize;
	
	// Add some margin to avoid trees at tile edges
	float Margin = CellSize * 2;
	TileMinX += Margin;
	TileMaxX -= Margin;
	TileMinY += Margin;
	TileMaxY -= Margin;
	
	// Calculate tile size after margin
	float TileWidth = TileMaxX - TileMinX;
	float TileHeight = TileMaxY - TileMinY;
	
	// Use tree-specific Perlin noise to generate deterministic position within tile
	FVector2D NoiseInput = FVector2D(TileKey.X, TileKey.Y);
	
	float NoiseX = TreePerlinNoise(NoiseInput, 0.15f, FVector2D(100.0f, 100.0f));
	float NoiseY = TreePerlinNoise(NoiseInput, 0.15f, FVector2D(1000.0f, 1000.0f));
	
	// Convert noise values (-1 to 1) to normalized values (0 to 1)
	float NormalizedX = (NoiseX + 1.0f) * 0.5f;
	float NormalizedY = (NoiseY + 1.0f) * 0.5f;
	
	// Map to tile boundaries
	float PositionX = TileMinX + (NormalizedX * TileWidth);
	float PositionY = TileMinY + (NormalizedY * TileHeight);
	
	return FVector(PositionX, PositionY, 0);
}

bool ALandscapeGenerator::IsValidTreePosition(const FVector& Position, const TArray<FVector>& ExistingTrees)
{
	for (const FVector& ExistingTree : ExistingTrees)
	{
		float Distance = FVector::Dist2D(Position, ExistingTree);
		if (Distance < MinTreeDistance)
		{
			return false;
		}
	}
	return true;
}

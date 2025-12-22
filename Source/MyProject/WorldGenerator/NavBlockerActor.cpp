// Fill out your copyright notice in the Description page of Project Settings.

#include "NavBlockerActor.h"
#include "NavAreas/NavArea_Null.h"
#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Async/Async.h"
#include "NavigationSystem.h"
#include "PhysicsEngine/BodySetup.h"

ANavBlockerActor::ANavBlockerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root scene component
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);
	
	// Cubes will be created dynamically in SetBlockingExtent
}

UBoxComponent* ANavBlockerActor::CreateBlockingCube(const FVector& LocalPosition, float HalfSize)
{
	UBoxComponent* Cube = NewObject<UBoxComponent>(this);
	Cube->SetupAttachment(RootScene);
	Cube->SetRelativeLocation(LocalPosition);
	Cube->SetBoxExtent(FVector(HalfSize, HalfSize, HalfSize));
	
	// Configure for navigation blocking only (no physics collision)
	Cube->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Cube->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	// Set the area class to NavArea_Null - this makes it unwalkable
	Cube->SetAreaClassOverride(UNavArea_Null::StaticClass());
	
	// Make sure it affects navigation
	Cube->SetCanEverAffectNavigation(true);
	Cube->bDynamicObstacle = true;
	
	Cube->RegisterComponent();
	
	return Cube;
}

void ANavBlockerActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Disable tick if debug visualization is off
	SetActorTickEnabled(bShowDebugVisualization);
}

void ANavBlockerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bShowDebugVisualization)
	{
		DrawDebugVisualization();
	}
}

void ANavBlockerActor::SetBlockingExtent(const FVector& Extent, const FVector& MeshCenterOffset)
{
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("NavBlocker::SetBlockingExtent - No world!"));
		return;
	}
	
	// Clear any existing cubes
	for (UBoxComponent* Cube : BlockingCubes)
	{
		if (Cube)
		{
			Cube->DestroyComponent();
		}
	}
	BlockingCubes.Empty();
	PendingCubePositions.Empty();
	
	// Validate extent
	if (Extent.IsNearlyZero())
	{
		UE_LOG(LogTemp, Error, TEXT("NavBlocker::SetBlockingExtent - Invalid extent (near zero)!"));
		return;
	}
	
	// Add padding to extent
	FVector PaddedExtent = Extent + FVector(BoundsPadding, BoundsPadding, 0.0f);
	
	// Calculate full height including extensions
	float TotalHeight = (Extent.Z * 2.0f) + HeightOffset + DownwardExtension;
	
	// Half size for each cube
	float HalfCube = CubeSize * 0.5f;
	
	// Calculate how many cubes we need in each dimension
	int32 NumCubesX = FMath::CeilToInt((PaddedExtent.X * 2.0f) / CubeSize);
	int32 NumCubesY = FMath::CeilToInt((PaddedExtent.Y * 2.0f) / CubeSize);
	int32 NumCubesZ = FMath::CeilToInt(TotalHeight / CubeSize);
	
	// Ensure at least 1 cube in each dimension
	NumCubesX = FMath::Max(1, NumCubesX);
	NumCubesY = FMath::Max(1, NumCubesY);
	NumCubesZ = FMath::Max(1, NumCubesZ);
	
	// Calculate starting positions (bottom-left-back corner)
	float StartX = -PaddedExtent.X + HalfCube;
	float StartY = -PaddedExtent.Y + HalfCube;
	float StartZ = -Extent.Z - DownwardExtension + HalfCube;
	
	// Build list of all positions (will be processed in batches)
	for (int32 ix = 0; ix < NumCubesX; ix++)
	{
		for (int32 iy = 0; iy < NumCubesY; iy++)
		{
			for (int32 iz = 0; iz < NumCubesZ; iz++)
			{
				FVector LocalPos(
					StartX + ix * CubeSize,
					StartY + iy * CubeSize,
					StartZ + iz * CubeSize
				);
				PendingCubePositions.Add(LocalPos);
			}
		}
	}
	
	// Store parameters for async processing
	ProcessedCubeIndex = 0;
	StoredHalfCube = HalfCube;
	TotalCubesToProcess = PendingCubePositions.Num();
	
	UE_LOG(LogTemp, Warning, TEXT("NavBlocker queued %d cubes (grid: %dx%dx%d, cube size: %.1f) for mesh extent: %s"), 
		TotalCubesToProcess, NumCubesX, NumCubesY, NumCubesZ, CubeSize, *Extent.ToString());
	
	// Start async cube creation (no collision testing needed)
	bIsProcessingCubes = true;
	GetWorld()->GetTimerManager().SetTimer(
		CubeProcessingTimerHandle,
		this,
		&ANavBlockerActor::ProcessSimpleCubeBatch,
		0.016f, // Process once per frame at 60fps
		true
	);
}

void ANavBlockerActor::SetBlockingExtentFromMesh(const FVector& Extent, UStaticMesh* Mesh, float MeshScale, const FVector& MeshWorldLocation)
{
	// Skip processing if flagged
	if (!bShouldProcess)
	{
		UE_LOG(LogTemp, Warning, TEXT("NavBlocker: Skipped (bShouldProcess = false)"));
		return;
	}

	// Clear any existing cubes
	for (UBoxComponent* Cube : BlockingCubes)
	{
		if (Cube)
		{
			Cube->DestroyComponent();
		}
	}
	BlockingCubes.Empty();
	CubesToCheck.Empty();
	PendingCubePositions.Empty();
	
	if (!Mesh || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("NavBlocker: No mesh or world provided, falling back to full extent"));
		SetBlockingExtent(Extent);
		return;
	}
	
	// Store parameters and start immediately (processing will be spread across frames)
	DelayedExtent = Extent;
	DelayedMesh = Mesh;
	DelayedMeshScale = MeshScale;
	DelayedMeshWorldLocation = MeshWorldLocation;  // Store the actual world location
	
	// Start immediately - cube creation is already async/batched
	UE_LOG(LogTemp, Warning, TEXT("NavBlocker: Starting generation for extent %s, MeshWorldLocation: %s"), 
		*Extent.ToString(), *MeshWorldLocation.ToString());
	StartBlockingExtentFromMesh();
}

void ANavBlockerActor::StartBlockingExtentFromMesh()
{
	bPendingDelayedStart = false;
	
	if (!DelayedMesh || !GetWorld())
	{
		return;
	}
	
	// If skipping collision test, just use bounding box approach (faster)
	if (bSkipCollisionTest)
	{
		// Use simple bounding box filling - much faster, no per-cube collision test
		SetBlockingExtent(DelayedExtent, FVector::ZeroVector);
		DelayedMesh = nullptr;
		return;
	}
	
	// Use the actual world location where the house mesh is placed
	// This is passed directly from HouseSpawner - it's the SpawnLocation used for the instance
	FVector MeshOriginWorld = DelayedMeshWorldLocation;
	
	UE_LOG(LogTemp, Warning, TEXT("NavBlocker: ActorLocation: %s, MeshOriginWorld (actual house location): %s"), 
		*GetActorLocation().ToString(), *MeshOriginWorld.ToString());
	
	// Create and keep the temporary static mesh component for collision testing
	// Place it at the SAME location as the actual house instance
	TempMeshComponent = NewObject<UStaticMeshComponent>(this);
	TempMeshComponent->SetStaticMesh(DelayedMesh);
	TempMeshComponent->SetWorldLocation(MeshOriginWorld);
	TempMeshComponent->SetWorldRotation(GetActorRotation());
	TempMeshComponent->SetWorldScale3D(FVector(DelayedMeshScale));
	TempMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TempMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	// Make it visible for debugging - should overlap exactly with the real house
	TempMeshComponent->SetVisibility(bShowDebugVisualization);
	TempMeshComponent->RegisterComponent();
	
	// Get the actual world-space bounds of the collision AFTER the component is placed and scaled
	// This is the simplest and most reliable way to get where the collision actually is
	FBox WorldCollisionBounds = TempMeshComponent->Bounds.GetBox();
	
	UE_LOG(LogTemp, Warning, TEXT("NavBlocker: TempMesh WorldBounds Min: %s, Max: %s"), 
		*WorldCollisionBounds.Min.ToString(), *WorldCollisionBounds.Max.ToString());
	
	FVector CollisionCenterWorld = WorldCollisionBounds.GetCenter();
	FVector CollisionExtentWorld = WorldCollisionBounds.GetExtent();
	
	UE_LOG(LogTemp, Warning, TEXT("NavBlocker: CollisionCenterWorld: %s, CollisionExtentWorld: %s"), 
		*CollisionCenterWorld.ToString(), *CollisionExtentWorld.ToString());
	
	// Half size for each cube
	float HalfCube = CubeSize * 0.5f;
	
	// Calculate how many cubes we need in each dimension based on world-space bounds
	int32 NumCubesX = FMath::CeilToInt((CollisionExtentWorld.X * 2.0f) / CubeSize);
	int32 NumCubesY = FMath::CeilToInt((CollisionExtentWorld.Y * 2.0f) / CubeSize);
	int32 NumCubesZ = FMath::CeilToInt((CollisionExtentWorld.Z * 2.0f) / CubeSize);
	
	// Ensure at least 1 cube in each dimension
	NumCubesX = FMath::Max(1, NumCubesX);
	NumCubesY = FMath::Max(1, NumCubesY);
	NumCubesZ = FMath::Max(1, 0.2f * NumCubesZ);
	
	// The grid needs to be centered on where the collision actually is in world space
	// Convert world collision center to local space relative to NavBlocker actor
	FVector CollisionOffsetFromActor = GetActorQuat().UnrotateVector(CollisionCenterWorld - GetActorLocation());
	
	UE_LOG(LogTemp, Warning, TEXT("NavBlocker: CollisionOffsetFromActor: %s"), *CollisionOffsetFromActor.ToString());
	
	// Grid start positions in local space, centered on the collision
	float StartX = CollisionOffsetFromActor.X - CollisionExtentWorld.X + HalfCube;
	float StartY = CollisionOffsetFromActor.Y - CollisionExtentWorld.Y + HalfCube;
	float StartZ = CollisionOffsetFromActor.Z - CollisionExtentWorld.Z + HalfCube;
	
	// Store parameters for async processing
	ProcessedCubeIndex = 0;
	StoredHalfCube = HalfCube;
	StoredTraceDistance = CollisionExtentWorld.GetMax() * 3.0f;
	StoredActorTransform = GetActorTransform();
	
	// Build list of all grid positions to check (we'll create cubes only where mesh collision exists)
	for (int32 ix = 0; ix < NumCubesX; ix++)
	{
		for (int32 iy = 0; iy < NumCubesY; iy++)
		{
			for (int32 iz = 0; iz < NumCubesZ; iz++)
			{
				FVector LocalPos(
					StartX + ix * CubeSize,
					StartY + iy * CubeSize,
					StartZ + iz * CubeSize
				);
				PendingCubePositions.Add(LocalPos);
			}
		}
	}
	
	TotalCubesToProcess = PendingCubePositions.Num();
	
	UE_LOG(LogTemp, Warning, TEXT("NavBlocker scanning %d positions for mesh collision (grid: %dx%dx%d)"), 
		TotalCubesToProcess, NumCubesX, NumCubesY, NumCubesZ);
	
	// Start async collision scan - create cubes only where collision exists
	bIsProcessingCubes = true;
	GetWorld()->GetTimerManager().SetTimer(
		CubeProcessingTimerHandle,
		this,
		&ANavBlockerActor::ProcessCollisionScanBatch,
		0.016f, // Process once per frame at 60fps to spread load
		true    // Looping
	);
}

void ANavBlockerActor::SetHeightOffset(float NewHeightOffset)
{
	HeightOffset = NewHeightOffset;
}

void ANavBlockerActor::ProcessSimpleCubeBatch()
{
	if (!bIsProcessingCubes || !GetWorld())
	{
		FinishCubeProcessing();
		return;
	}
	
	// Safety check - if no pending positions, we're done
	if (PendingCubePositions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("NavBlocker::ProcessSimpleCubeBatch - No pending positions!"));
		FinishCubeProcessing();
		return;
	}
	
	// Create a limited number of cubes per frame to avoid stutter
	const int32 MaxCubesPerBatch = 15;
	int32 CubesCreatedThisBatch = 0;
	
	while (ProcessedCubeIndex < PendingCubePositions.Num() && CubesCreatedThisBatch < MaxCubesPerBatch)
	{
		const FVector& LocalPos = PendingCubePositions[ProcessedCubeIndex];
		UBoxComponent* Cube = CreateBlockingCube(LocalPos, StoredHalfCube);
		if (Cube)
		{
			BlockingCubes.Add(Cube);
		}
		
		ProcessedCubeIndex++;
		CubesCreatedThisBatch++;
	}
	
	// Check if we're done
	if (ProcessedCubeIndex >= PendingCubePositions.Num())
	{
		FinishCubeProcessing();
	}
}

void ANavBlockerActor::DrawDebugVisualization()
{
	if (!GetWorld())
	{
		return;
	}
	
	FQuat ActorRotation = GetActorQuat();
	FVector ActorLocation = GetActorLocation();
	
	// Draw each cube
	for (UBoxComponent* Cube : BlockingCubes)
	{
		if (Cube)
		{
			FVector BoxExtent = Cube->GetScaledBoxExtent();
			FVector BoxCenter = ActorLocation + ActorRotation.RotateVector(Cube->GetRelativeLocation());
			
			DrawDebugBox(
				GetWorld(),
				BoxCenter,
				BoxExtent,
				ActorRotation,
				DebugColor,
				false,
				-1.0f,
				0,
				1.0f
			);
		}
	}
	
	// Draw text label
	DrawDebugString(GetWorld(), ActorLocation + FVector(0, 0, 200.0f), 
		FString::Printf(TEXT("NavBlocker (%d cubes)"), BlockingCubes.Num()), 
		nullptr, DebugColor, 0.0f, true);
}

void ANavBlockerActor::ProcessCollisionScanBatch()
{
	if (!bIsProcessingCubes || !TempMeshComponent)
	{
		FinishCubeProcessing();
		return;
	}
	
	// Process a batch of positions per frame - reduced to minimize stutter
	const int32 PositionsPerBatch = FMath::Max(10, CubesPerFrame / 10);
	int32 ProcessedThisBatch = 0;
	int32 CubesCreatedThisBatch = 0;
	const int32 MaxCubesPerBatch = 20; // Limit cube creation per frame to reduce stutter
	
	while (ProcessedCubeIndex < PendingCubePositions.Num() && 
	       ProcessedThisBatch < PositionsPerBatch &&
	       CubesCreatedThisBatch < MaxCubesPerBatch)
	{
		const FVector& LocalPos = PendingCubePositions[ProcessedCubeIndex];
		
		// Convert local position to world position
		FVector WorldPos = GetActorLocation() + GetActorQuat().RotateVector(LocalPos);
		
		// Test if this position is INSIDE the mesh (not just overlapping surface)
		bool bIsInside = IsPointInsideMesh(WorldPos);
		
		if (bIsInside)
		{
			// This position is inside the mesh - create a blocking cube here
			UBoxComponent* Cube = CreateBlockingCube(LocalPos, StoredHalfCube);
			BlockingCubes.Add(Cube);
			CubesCreatedThisBatch++;
		}
		
		ProcessedCubeIndex++;
		ProcessedThisBatch++;
	}
	
	// Check if we're done
	if (ProcessedCubeIndex >= PendingCubePositions.Num())
	{
		FinishCubeProcessing();
	}
}

bool ANavBlockerActor::IsPointInsideMesh(const FVector& WorldPos) const
{
	if (!TempMeshComponent || !GetWorld())
	{
		return false;
	}
	
	// Use overlap test with a small sphere to check if point is inside collision
	// This is more reliable than GetClosestPointOnCollision for "inside" detection
	FCollisionShape SmallSphere = FCollisionShape::MakeSphere(1.0f);
	
	// Check if the small sphere at WorldPos overlaps with the TempMeshComponent
	bool bOverlaps = TempMeshComponent->OverlapComponent(
		WorldPos,
		FQuat::Identity,
		SmallSphere
	);
	
	return bOverlaps;
}

void ANavBlockerActor::FinishCubeProcessing()
{
	bIsProcessingCubes = false;
	
	// Stop the timers
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CubeProcessingTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(DelayedStartTimerHandle);
	}
	
	// Destroy the temporary mesh
	if (TempMeshComponent)
	{
		TempMeshComponent->DestroyComponent();
		TempMeshComponent = nullptr;
	}
	
	// Clear stored mesh reference
	DelayedMesh = nullptr;
	
	// Clear pending data
	PendingCubePositions.Empty();
	CubesToCheck.Empty();
	
	UE_LOG(LogTemp, Warning, TEXT("NavBlocker finished: created %d cubes where collision was found (scanned %d positions)"), 
		BlockingCubes.Num(), TotalCubesToProcess);
	
	// Update navigation
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys)
	{
		NavSys->AddDirtyArea(GetComponentsBoundingBox(), ENavigationDirtyFlag::All);
	}
}

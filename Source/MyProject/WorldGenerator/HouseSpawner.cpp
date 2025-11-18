// Fill out your copyright notice in the Description page of Project Settings.


#include "HouseSpawner.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
AHouseSpawner::AHouseSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create a root component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Load house meshes
	struct FHouseMeshPath
	{
		const TCHAR* Path;
		int32 Index;
	};
	
	const FHouseMeshPath MeshPaths[] = {
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/BH1.BH1'"), 1 },
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/BH2.BH2'"), 2 },
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/BH3.BH3'"), 3 },
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/BH4.BH4'"), 4 },
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/BH5.BH5'"), 5 },
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/BH6.BH6'"), 6 },
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/BH7.BH7'"), 7 },
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/BH8.BH8'"), 8 },
		{ TEXT("/Script/Engine.StaticMesh'/Game/Models/Houses/scene/ReadyHouses/Tower.Tower'"), 9 }
	};
	
	for (const FHouseMeshPath& MeshPath : MeshPaths)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> HouseMesh(MeshPath.Path);
		if (HouseMesh.Succeeded())
		{
			HouseMeshes.Add(HouseMesh.Object);
			UE_LOG(LogTemp, Warning, TEXT("HouseSpawner::Constructor - House mesh %d loaded successfully"), MeshPath.Index);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HouseSpawner::Constructor - Failed to load house mesh %d!"), MeshPath.Index);
		}
	}
}

// Called when the game starts or when spawned
void AHouseSpawner::BeginPlay()
{
	Super::BeginPlay();
    // AutoReceiveInput = EAutoReceiveInput::Player0;
	
	UE_LOG(LogTemp, Warning, TEXT("HouseSpawner::BeginPlay - Starting initialization"));
	UE_LOG(LogTemp, Warning, TEXT("HouseSpawner::BeginPlay - HouseMeshes.Num() = %d"), HouseMeshes.Num());
	
	// Create instanced mesh components for each house type
	for (int32 i = 0; i < HouseMeshes.Num(); i++)
	{
		if (HouseMeshes[i])
		{
			UInstancedStaticMeshComponent* InstancedMesh = NewObject<UInstancedStaticMeshComponent>(this);
			InstancedMesh->SetStaticMesh(HouseMeshes[i]);
			InstancedMesh->SetupAttachment(RootComponent);
			InstancedMesh->RegisterComponent();
			HouseInstancedMeshes.Add(InstancedMesh);
			UE_LOG(LogTemp, Warning, TEXT("HouseSpawner::BeginPlay - Created instanced mesh %d"), i);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HouseSpawner::BeginPlay - HouseMesh %d is NULL!"), i);
		}
	}
	
	// Set up input component
	UE_LOG(LogTemp, Warning, TEXT("HouseSpawner::BeginPlay - Setting up input"));
	
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("HouseSpawner::BeginPlay - PlayerController found: %s"), *PC->GetName());
		EnableInput(PC);
        // InputComponent->Priority = -1;
		
		if (InputComponent)
		{
			InputComponent->BindAction("SpawnHouse", IE_Pressed, this, &AHouseSpawner::SpawnHouseInFrontOfPlayer);
			UE_LOG(LogTemp, Warning, TEXT("HouseSpawner::BeginPlay - Input bound successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HouseSpawner::BeginPlay - InputComponent is NULL after EnableInput!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HouseSpawner::BeginPlay - PlayerController not found!"));
	}
}

// Called every frame
void AHouseSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AHouseSpawner::SpawnHousesOnTile(const FIntPoint& TileKey)
{
	// TODO: Implement house spawning logic
}

void AHouseSpawner::RemoveHousesOnTile(const FIntPoint& TileKey)
{
	// TODO: Implement house removal logic
}

FVector AHouseSpawner::GetRandomPointOnTile(const FIntPoint& TileKey)
{
	// TODO: Implement random point generation
	return FVector::ZeroVector;
}

bool AHouseSpawner::IsValidHousePosition(const FVector& Position, const TArray<FVector>& ExistingHouses)
{
	// TODO: Implement position validation
	return true;
}

void AHouseSpawner::SpawnHouseInFrontOfPlayer()
{
	UE_LOG(LogTemp, Warning, TEXT("HouseSpawner::SpawnHouseInFrontOfPlayer - Called!"));
	
	if (HouseInstancedMeshes.Num() == 0 || HouseMeshes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No house meshes available to spawn - HouseInstancedMeshes: %d, HouseMeshes: %d"), 
			HouseInstancedMeshes.Num(), HouseMeshes.Num());
		return;
	}
	
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController is NULL!"));
		return;
	}
	
	APawn* PlayerPawn = PlayerController->GetPawn();
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerPawn is NULL!"));
		return;
	}
	
	// Get player location and forward vector
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	FVector PlayerForward = PlayerPawn->GetActorForwardVector();
	
	UE_LOG(LogTemp, Warning, TEXT("Player Location: %s, Forward: %s"), *PlayerLocation.ToString(), *PlayerForward.ToString());
	
	// Calculate spawn location in front of player
	FVector SpawnLocation = PlayerLocation + (PlayerForward * SpawnDistanceInFrontOfPlayer);
	
	// Random rotation around Z axis (decided before sampling to know corner positions)
	float RandomYaw = FMath::RandRange(0.0f, 360.0f);
	FRotator SpawnRotation = FRotator(0, RandomYaw, 0);
	
	// Sample 4 corners of the house to get proper ground alignment
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(PlayerPawn);
	QueryParams.AddIgnoredActor(this);
	
	// Generate corner offsets for all 4 corners
	TArray<FVector> CornerOffsets;
	for (int32 x = -1; x <= 1; x += 2)
	{
		for (int32 y = -1; y <= 1; y += 2)
		{
			CornerOffsets.Add(FVector(x * HouseBoundsRadius, y * HouseBoundsRadius, 0));
		}
	}
	
	TArray<FVector> CornerHitLocations;
	FVector AverageGroundNormal = FVector::ZeroVector;
	float MinHeight = FLT_MAX;
	float MaxHeight = -FLT_MAX;
	float SumHeight = 0.0f;
	int32 ValidHits = 0;
	
	// Trace each corner
	for (const FVector& CornerOffset : CornerOffsets)
	{
		// Rotate corner offset by spawn rotation
		FVector RotatedOffset = SpawnRotation.RotateVector(CornerOffset);
		FVector CornerWorldPos = SpawnLocation + RotatedOffset;
		
		FHitResult HitResult;
		FVector TraceStart = CornerWorldPos + FVector(0, 0, 1000.0f);
		FVector TraceEnd = CornerWorldPos - FVector(0, 0, 1000.0f);
		
		if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			CornerHitLocations.Add(HitResult.Location);
			AverageGroundNormal += HitResult.Normal;
			SumHeight += HitResult.Location.Z;
			MinHeight = FMath::Min(MinHeight, HitResult.Location.Z);
			MaxHeight = FMath::Max(MaxHeight, HitResult.Location.Z);
			ValidHits++;
		}
	}
	
	if (ValidHits == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No valid ground hits for house placement, skipping spawn"));
		return;
	}
	
	// Use lowest point or average based on setting
	float GroundHeight = bUseLowestPoint ? MinHeight : (SumHeight / ValidHits);
	
	// Apply pivot offset (negative offset moves house down, positive moves up)
	SpawnLocation.Z = GroundHeight - HousePivotZOffset;
	
	UE_LOG(LogTemp, Warning, TEXT("House placement - Hits: %d, Min: %.2f, Max: %.2f, Final Z: %.2f"), 
		ValidHits, MinHeight, MaxHeight, SpawnLocation.Z);
	
	// Optionally align house to terrain slope
	if (bAlignToTerrainSlope && ValidHits > 0)
	{
		AverageGroundNormal.Normalize();
		
		// Calculate rotation to align with terrain
		FVector UpVector = AverageGroundNormal;
		FVector ForwardVector = FVector::CrossProduct(FVector::RightVector, UpVector);
		ForwardVector.Normalize();
		
		// Apply the random yaw rotation to the forward vector
		FRotator YawRotation(0, RandomYaw, 0);
		ForwardVector = YawRotation.RotateVector(FVector::ForwardVector);
		
		// Make sure forward is perpendicular to up
		FVector RightVector = FVector::CrossProduct(UpVector, ForwardVector);
		RightVector.Normalize();
		ForwardVector = FVector::CrossProduct(RightVector, UpVector);
		
		SpawnRotation = UKismetMathLibrary::MakeRotFromXZ(ForwardVector, UpVector);
		
		UE_LOG(LogTemp, Warning, TEXT("Terrain alignment - Normal: %s, Rotation: %s"), 
			*AverageGroundNormal.ToString(), *SpawnRotation.ToString());
	}
	
	// Random scale
	float RandomScale = FMath::RandRange(HouseScaleMin, HouseScaleMax);
	FVector SpawnScale = FVector(RandomScale, RandomScale, RandomScale);
	
	// Create transform
	FTransform SpawnTransform(SpawnRotation, SpawnLocation, SpawnScale);
	
	// Pick a random house type from the array
	int32 RandomHouseIndex = FMath::RandRange(0, HouseInstancedMeshes.Num() - 1);
	
	UE_LOG(LogTemp, Warning, TEXT("Random selection - Total house types: %d, Selected index: %d"), 
		HouseInstancedMeshes.Num(), RandomHouseIndex);
	
	// Spawn the randomly selected house type
	if (HouseInstancedMeshes.IsValidIndex(RandomHouseIndex))
	{
		int32 InstanceIndex = HouseInstancedMeshes[RandomHouseIndex]->AddInstance(SpawnTransform);
		UE_LOG(LogTemp, Warning, TEXT("House spawned (type %d) at: %s with index: %d"), 
			RandomHouseIndex, *SpawnLocation.ToString(), InstanceIndex);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HouseInstancedMeshes[%d] is not valid!"), RandomHouseIndex);
	}
}

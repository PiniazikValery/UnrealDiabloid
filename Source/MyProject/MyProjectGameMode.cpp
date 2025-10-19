// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectGameMode.h"
#include "MyProjectCharacter.h"
#include "./WorldGenerator/LandscapeGenerator.h"
#include "./AWarmupManager.h"
#include "UObject/ConstructorHelpers.h"
#include "AI/NavigationSystemBase.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "Components/BrushComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"

AMyProjectGameMode::AMyProjectGameMode()
{
	DefaultPawnClass = AMyProjectCharacter::StaticClass();
}

void AMyProjectGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnLandscapeGenerator();
	SpawnEnemySpawner();
	SpawnWarmupManager();
}

void AMyProjectGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	UE_LOG(LogTemp, Warning, TEXT("PostLogin called for PlayerController: %s"), 
		NewPlayer ? *NewPlayer->GetName() : TEXT("NULL"));
	
	if (NewPlayer)
	{
		// Ensure InputComponent is created for this PlayerController
		EnsurePlayerControllerInputComponent(NewPlayer);
		
		// Ensure the player has a valid pawn
		if (!NewPlayer->GetPawn())
		{
			UE_LOG(LogTemp, Warning, TEXT("Player has no pawn, restarting player: %s"), *NewPlayer->GetName());
			RestartPlayer(NewPlayer);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Player already has pawn: %s -> %s"), 
				*NewPlayer->GetName(), *NewPlayer->GetPawn()->GetName());
		}
	}
}

void AMyProjectGameMode::RestartPlayer(AController* NewPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("RestartPlayer called for Controller: %s"), 
		NewPlayer ? *NewPlayer->GetName() : TEXT("NULL"));
	
	Super::RestartPlayer(NewPlayer);
	
	if (NewPlayer && NewPlayer->GetPawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("RestartPlayer success: %s -> %s"), 
			*NewPlayer->GetName(), *NewPlayer->GetPawn()->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RestartPlayer failed for Controller: %s"), 
			NewPlayer ? *NewPlayer->GetName() : TEXT("NULL"));
	}
}

void AMyProjectGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("HandleStartingNewPlayer called for: %s"), 
		NewPlayer ? *NewPlayer->GetName() : TEXT("NULL"));
	
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	if (NewPlayer)
	{
		// Ensure InputComponent is created before we proceed
		EnsurePlayerControllerInputComponent(NewPlayer);
		
		// Ensure the player gets a pawn immediately
		if (!NewPlayer->GetPawn())
		{
			UE_LOG(LogTemp, Warning, TEXT("HandleStartingNewPlayer: Player has no pawn, creating one"));
			RestartPlayer(NewPlayer);
		}
	}
}

void AMyProjectGameMode::SpawnCharacterAtReachablePointTest()
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);

	if (PlayerCharacter)
	{
		FVector				 PlayerLocation = PlayerCharacter->GetActorLocation();
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		FVector				 ReachablePoint;

		if (NavSys)
		{
			FNavLocation NavLocation;
			if (NavSys->GetRandomReachablePointInRadius(PlayerLocation, 2000, NavLocation))
			{
				ReachablePoint = NavLocation.Location;
			}
		}

		if (ReachablePoint != FVector::ZeroVector)
		{
			FRotator			  SpawnRotation = FRotator::ZeroRotator;
			FActorSpawnParameters SpawnParams;
			ReachablePoint.Z = ReachablePoint.Z + 100;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = GetInstigator();

			AMyProjectCharacter* SpawnedCharacter = GetWorld()->SpawnActor<AMyProjectCharacter>(AMyProjectCharacter::StaticClass(), ReachablePoint, SpawnRotation, SpawnParams);
			if (SpawnedCharacter)
			{
				SpawnedCharacter->PossessAIController(AMyAIController::StaticClass());
				// Do something with the spawned character if needed
			}
		}
	}
}

void AMyProjectGameMode::SpawnLandscapeGenerator()
{
	if (UWorld* World = GetWorld())
	{
		FVector	 SpawnLocation(0.0f, 0.0f, 100.0f); // A small offset to avoid collision issues
		FRotator SpawnRotation(0.0f, 0.0f, 0.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this; // Optional
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		World->SpawnActor<ALandscapeGenerator>(
			ALandscapeGenerator::StaticClass(),
			SpawnLocation,
			SpawnRotation,
			SpawnParams);
	}
}

void AMyProjectGameMode::SetupNavigation()
{
	UWorld* World = GetWorld();
	if (!World)
		return;
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSystem)
	{
		UE_LOG(LogTemp, Error, TEXT("Navigation system is NULL!"));
		return;
	}
	ANavigationData* NavData = NavSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (!NavData)
	{
		UE_LOG(LogTemp, Warning, TEXT("No existing NavMesh found, creating a new one..."));

		// Spawn the RecastNavMesh
		ARecastNavMesh* NavMesh = World->SpawnActor<ARecastNavMesh>();
		if (NavMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("Recast NavMesh created successfully!"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create Recast NavMesh!"));
			return;
		}
	}
	FActorSpawnParameters SpawnParams;
	ANavMeshBoundsVolume* NavMeshVolume = GetWorld()->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, 0), FRotator::ZeroRotator);

	if (NavMeshVolume)
	{
		UE_LOG(LogTemp, Warning, TEXT("NavMeshBoundsVolume spawned successfully!"));

		auto* NavMeshBrush = NavMeshVolume->GetBrushComponent();
		NavMeshBrush->SetMobility(EComponentMobility::Movable);
		NavMeshBrush->Bounds.BoxExtent = { 2000.f, 2000.f, 2000.f };
		NavSystem->OnNavigationBoundsAdded(NavMeshVolume);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn NavMeshBoundsVolume!"));
	}
}

void AMyProjectGameMode::SpawnEnemySpawner()
{
	if (UWorld* World = GetWorld())
	{
		FVector	 SpawnLocation = FVector(0.0f, 0.0f, 100.0f); // Например, центр мира
		FRotator SpawnRotation = FRotator::ZeroRotator;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AAEnemySpawner* Spawner = World->SpawnActor<AAEnemySpawner>(
			AAEnemySpawner::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	}
}

void AMyProjectGameMode::SpawnWarmupManager()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AWarmupManager>(AWarmupManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

void AMyProjectGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogTemp, Warning, TEXT("Logout called for Controller: %s"), 
		Exiting ? *Exiting->GetName() : TEXT("NULL"));
	
	Super::Logout(Exiting);
}

void AMyProjectGameMode::EnsurePlayerControllerInputComponent(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("EnsurePlayerControllerInputComponent: PlayerController is NULL"));
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("EnsurePlayerControllerInputComponent called for: %s"), *PlayerController->GetName());
	UE_LOG(LogTemp, Warning, TEXT("EnsurePlayerControllerInputComponent: InputComponent was %s"), 
		PlayerController->InputComponent ? TEXT("Valid") : TEXT("NULL"));
	
	// Force enable input for this PlayerController if it doesn't have InputComponent
	if (!PlayerController->InputComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enabling input for PlayerController: %s"), *PlayerController->GetName());
		
		// Enable input processing for this PlayerController
		PlayerController->EnableInput(PlayerController);
		
		// Also try to manually create the input component with the enhanced input class
		if (!PlayerController->InputComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("Creating Enhanced InputComponent manually for: %s"), *PlayerController->GetName());
			PlayerController->CreateInputComponent(UEnhancedInputComponent::StaticClass());
		}
		
		if (PlayerController->InputComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("Successfully ensured InputComponent for: %s"), *PlayerController->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create InputComponent for: %s"), *PlayerController->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("InputComponent already exists for: %s"), *PlayerController->GetName());
	}
}

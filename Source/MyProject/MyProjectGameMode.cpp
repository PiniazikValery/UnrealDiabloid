// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectGameMode.h"
#include "MyProjectCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMyProjectGameMode::AMyProjectGameMode()
{
	DefaultPawnClass = AMyProjectCharacter::StaticClass();
}

void AMyProjectGameMode::BeginPlay()
{
	Super::BeginPlay();
	/*SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();*/
}

void AMyProjectGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	//UE_LOG(LogTemp, Log, TEXT("New player spawned"));
	//if (NewPlayer)
	//{
	//	UE_LOG(LogTemp, Log, TEXT("NewPlayer here"))
	//	// Define spawn location (you can make this dynamic or randomized)
	//	FVector	 SpawnLocation = FVector(0.0f, 0.0f, 500.0f); // Example: Spawn at the origin, above ground
	//	FRotator SpawnRotation = FRotator::ZeroRotator;

	//	// Spawn parameters
	//	FActorSpawnParameters SpawnParams;
	//	SpawnParams.Owner = NewPlayer;

	//	// Spawn the character
	//	AMyProjectCharacter* SpawnedCharacter = GetWorld()->SpawnActor<AMyProjectCharacter>(
	//		AMyProjectCharacter::StaticClass(),
	//		SpawnLocation,
	//		SpawnRotation,
	//		SpawnParams);

	//	if (SpawnedCharacter)
	//	{
	//		UE_LOG(LogTemp, Log, TEXT("SpawnedCharacter here"))
	//		// Assign the PlayerController to possess the spawned character
	//		NewPlayer->Possess(SpawnedCharacter);

	//		UE_LOG(LogTemp, Log, TEXT("PlayerController %s now possesses %s"),
	//			*NewPlayer->GetName(), *SpawnedCharacter->GetName());
	//	}
	//	else
	//	{
	//		UE_LOG(LogTemp, Error, TEXT("Failed to spawn character for PlayerController %s"), *NewPlayer->GetName());
	//	}
	//}
}

void AMyProjectGameMode::SpawnCharacterAtReachablePointTest()
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);

	if (PlayerCharacter)
	{
		UE_LOG(LogTemp, Log, TEXT("PlayerCharacter found"));
		FVector				 PlayerLocation = PlayerCharacter->GetActorLocation();
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		FVector				 ReachablePoint;

		if (NavSys)
		{
			UE_LOG(LogTemp, Log, TEXT("Navigation system found"));
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
			SpawnParams.Owner = this;
			SpawnParams.Instigator = GetInstigator();

			AMyProjectCharacter* SpawnedCharacter = GetWorld()->SpawnActor<AMyProjectCharacter>(AMyProjectCharacter::StaticClass(), ReachablePoint, SpawnRotation, SpawnParams);
			if (SpawnedCharacter)
			{
				SpawnedCharacter->PossessAIController(AMyAIController::StaticClass());
				UE_LOG(LogTemp, Log, TEXT("Character spawned"));
				// Do something with the spawned character if needed
			}
		}
	}
}

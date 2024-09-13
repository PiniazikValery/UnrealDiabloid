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
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
	SpawnCharacterAtReachablePointTest();
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
				SpawnedCharacter->InitializeCharacter(true, AMyAIController::StaticClass());
				UE_LOG(LogTemp, Log, TEXT("Character spawned"));
				// Do something with the spawned character if needed
			}
		}
	}
}

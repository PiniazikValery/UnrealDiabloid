// Fill out your copyright notice in the Description page of Project Settings.

#include "AEnemySpawner.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

// Sets default values
AAEnemySpawner::AAEnemySpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Safe place to assign default class (after vtables are initialized)
	if (!EnemyClass)
	{
		EnemyClass = AEnemyCharacter::StaticClass();
	}
}

// Called when the game starts or when spawned
void AAEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!EnemyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemyClass not assigned in EnemySpawner!"));
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle, this, &AAEnemySpawner::SpawnWave, SpawnInterval, true);
}

void AAEnemySpawner::SpawnWave()
{
	for (int32 i = 0; i < EnemiesPerWave; ++i)
	{
		EEnemyType Type = static_cast<EEnemyType>(FMath::RandRange(1, 3));
		SpawnSingleEnemy(Type);
	}
}

void AAEnemySpawner::SpawnSingleEnemy(EEnemyType Type)
{
	if (ActiveEnemyCount >= MaxEnemies)
	{
		return;
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!PlayerCharacter)
		return;

	FVector				 PlayerLocation = PlayerCharacter->GetActorLocation();
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation		 NavLocation;

	if (NavSys && NavSys->GetRandomReachablePointInRadius(PlayerLocation, SpawnRadius, NavLocation))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AEnemyCharacter* Spawned = GetWorld()->SpawnActor<AEnemyCharacter>(
			EnemyClass, NavLocation.Location + FVector(0, 0, 100), FRotator::ZeroRotator, SpawnParams);

		if (Spawned)
		{
			Spawned->SetEnemyType(Type);
			Spawned->PossessAIController(AMyAIController::StaticClass());

			++ActiveEnemyCount;
			Spawned->OnDestroyed.AddDynamic(this, &AAEnemySpawner::OnEnemyDestroyed);
		}
	}
}

void AAEnemySpawner::OnEnemyDestroyed(AActor* DestroyedEnemy)
{
	--ActiveEnemyCount;
	ActiveEnemyCount = FMath::Max(ActiveEnemyCount, 0);
}

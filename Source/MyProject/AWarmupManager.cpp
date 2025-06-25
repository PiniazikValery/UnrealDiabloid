// Fill out your copyright notice in the Description page of Project Settings.

#include "AWarmupManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

// Sets default values
AWarmupManager::AWarmupManager()
{
	PrimaryActorTick.bCanEverTick = false; // Tick не нужен
}

void AWarmupManager::BeginPlay()
{
	Super::BeginPlay();

	if (!ProjectileClassToWarmup)
	{
		UE_LOG(LogTemp, Error, TEXT("ProjectileClassToWarmup is not set in WarmupManager!"));
		return;
	}

	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!PlayerChar)
	{
		UE_LOG(LogTemp, Warning, TEXT("No player character found for projectile warmup."));
		return;
	}

	// Спавним около персонажа — например, немного позади и ниже
	FVector	 SpawnLocation = PlayerChar->GetActorLocation() + PlayerChar->GetActorForwardVector() * 100.f;
	//FVector	 SpawnLocation = FVector(100000.f, 100000.f, 100000.f);
	FRotator SpawnRotation = PlayerChar->GetActorRotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();

	AMageProjectile* WarmupProjectile = GetWorld()->SpawnActor<AMageProjectile>(ProjectileClassToWarmup, SpawnLocation, SpawnRotation, SpawnParams);

	if (WarmupProjectile)
	{
		WarmupProjectile->SetActorHiddenInGame(true);
		WarmupProjectile->SetActorEnableCollision(false);
		//WarmupProjectile->SetLifeSpan(0.1f); // Уничтожится сам через кадр-два
		FVector LaunchDirection = SpawnRotation.Vector();
		WarmupProjectile->ProjectileMovement->Velocity = LaunchDirection * WarmupProjectile->ProjectileMovement->InitialSpeed;

		UE_LOG(LogTemp, Warning, TEXT("MageProjectile preloaded near the player."));
	}
}

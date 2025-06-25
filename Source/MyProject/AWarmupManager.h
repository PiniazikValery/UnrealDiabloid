// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "./Projectiles/MageProjectile.h"
#include "AWarmupManager.generated.h"


class AMageProjectile;

UCLASS()
class MYPROJECT_API AWarmupManager : public AActor
{
	GENERATED_BODY()

public:
	AWarmupManager();

protected:
	virtual void BeginPlay() override;

public:
	// Класс снаряда, который нужно прогреть
	UPROPERTY(EditAnywhere, Category = "Warmup")
	TSubclassOf<AMageProjectile> ProjectileClassToWarmup = AMageProjectile::StaticClass();
};
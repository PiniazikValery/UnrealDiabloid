// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "./MyProjectCharacter.h"
#include "EnemyCharacter.generated.h"

/**
 * 
 */
UCLASS()
class MYPROJECT_API AEnemyCharacter : public AMyProjectCharacter
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void SetEnemyType(EEnemyType NewType);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	EEnemyType GetEnemyType() const { return EnemyType; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	EEnemyType EnemyType = EEnemyType::E_None;

	void ConfigureEnemyByType();
};

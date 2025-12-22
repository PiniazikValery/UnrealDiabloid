// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassCommonFragments.h"
#include "EnemyFragments.h"
#include "EnemyTrait.generated.h"

/**
 * Trait that configures an entity as an enemy
 * This is the equivalent of setting up your AIController + Character in the old system
 */
UCLASS(meta = (DisplayName = "Enemy Trait"))
class MYPROJECT_API UEnemyTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	// Editor-configurable parameters (like UPROPERTY in your old Character class)
	
	UPROPERTY(EditAnywhere, Category = "Enemy|Movement")
	float MovementSpeed = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Movement")
	float RotationSpeed = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Movement")
	float AcceptanceRadius = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
	float AttackRange = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
	float AttackInterval = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
	float AttackDamage = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Stats")
	float MaxHealth = 100.0f;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

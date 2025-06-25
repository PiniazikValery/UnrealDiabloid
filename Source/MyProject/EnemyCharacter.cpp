// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyCharacter.h"

void AEnemyCharacter::SetEnemyType(EEnemyType NewType)
{
	if (EnemyType != NewType)
	{
		EnemyType = NewType;
		ConfigureEnemyByType();
	}
}

void AEnemyCharacter::ConfigureEnemyByType()
{
	switch (EnemyType)
	{
		case EEnemyType::E_Melee:
			GetCharacterMovement()->MaxWalkSpeed = 550.f;
			GetCharacterMovement()->GroundFriction = 10;
			GetCharacterMovement()->BrakingDecelerationWalking = 1000000;
			break;
		case EEnemyType::E_Ranged:
			GetCharacterMovement()->MaxWalkSpeed = 400;
			break;
		case EEnemyType::E_Tank:
			GetCharacterMovement()->MaxWalkSpeed = 350.f;
			break;
		default:
			break;
	}
}

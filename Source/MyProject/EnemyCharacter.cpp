// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyCharacter.h"
#include "Components/CapsuleComponent.h"

AEnemyCharacter::AEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set enemy-specific melee damage on CombatComponent
	if (CombatComponent)
	{
		CombatComponent->MeleeDamage = 0.5f;
	}
	// Load zombie attack montage (you'll need to create a montage from the animation sequence)
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ZombieAttackMontageAsset(TEXT("/Game/Characters/Mannequins/Animations/Attack/ZombieAttack_Montage.ZombieAttack_Montage"));
	if (ZombieAttackMontageAsset.Succeeded())
	{
		ZombieAttackMontage = ZombieAttackMontageAsset.Object;
	}
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AEnemyCharacter::PlayZombieAttack()
{
	if (ZombieAttackMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		AnimInstance->Montage_Play(ZombieAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);
		
		// Play on specific slot (make sure the slot name matches your setup)
		FName SlotName = FName("UpperBody");
		AnimInstance->Montage_SetPosition(ZombieAttackMontage, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ZombieAttackMontage is null or AnimInstance not available"));
	}
}

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
			GetCharacterMovement()->MaxWalkSpeed = 250.f;
			// GetCharacterMovement()->GroundFriction = 10;
			// GetCharacterMovement()->BrakingDecelerationWalking = 1000000;
			break;
		case EEnemyType::E_Ranged:
			GetCharacterMovement()->MaxWalkSpeed = 250.f;
			break;
		case EEnemyType::E_Tank:
			GetCharacterMovement()->MaxWalkSpeed = 250.f;
			break;
		default:
			break;
	}
}

void AEnemyCharacter::HandleDeath()
{
	// Call parent death handling (disables movement)
	Super::HandleDeath();
	
	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// Destroy the enemy after a short delay (0.1 second)
	SetLifeSpan(0.1f);
}

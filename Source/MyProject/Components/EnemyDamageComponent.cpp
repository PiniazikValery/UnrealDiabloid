// EnemyDamageComponent.cpp

#include "EnemyDamageComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"

UEnemyDamageComponent::UEnemyDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyDamageComponent::SetPendingDamage(AActor* InTargetActor, float InDamage, const FVector& InAttackerLocation)
{
	TargetActor = InTargetActor;
	PendingDamage = InDamage;
	AttackerLocation = InAttackerLocation;
	bHasPendingDamage = (InTargetActor != nullptr && InDamage > 0.0f);
}

bool UEnemyDamageComponent::ApplyPendingDamage()
{
	if (!bHasPendingDamage)
	{
		return false;
	}

	AActor* Target = TargetActor.Get();
	if (Target && Target->CanBeDamaged())
	{
		// Apply damage using Unreal's damage system
		UGameplayStatics::ApplyDamage(
			Target,
			PendingDamage,
			nullptr,  // No instigator controller for mass entities
			GetOwner(),  // Damage causer is the skeletal mesh actor
			UDamageType::StaticClass()
		);

		UE_LOG(LogTemp, Log, TEXT("EnemyDamageComponent: Applied %.1f damage to %s"),
			PendingDamage, *Target->GetName());

		ClearPendingDamage();
		return true;
	}

	ClearPendingDamage();
	return false;
}

void UEnemyDamageComponent::ClearPendingDamage()
{
	TargetActor = nullptr;
	PendingDamage = 0.0f;
	AttackerLocation = FVector::ZeroVector;
	bHasPendingDamage = false;
}

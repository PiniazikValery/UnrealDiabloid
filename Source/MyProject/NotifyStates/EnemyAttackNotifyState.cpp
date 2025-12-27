// EnemyAttackNotifyState.cpp

#include "EnemyAttackNotifyState.h"
#include "../Components/EnemyDamageComponent.h"

void UEnemyAttackNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	bDamageApplied = false;

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Find the EnemyDamageComponent on the pooled skeletal mesh actor
	UEnemyDamageComponent* DamageComp = Owner->FindComponentByClass<UEnemyDamageComponent>();
	if (DamageComp && DamageComp->HasPendingDamage())
	{
		// Apply the pending damage
		bDamageApplied = DamageComp->ApplyPendingDamage();
	}
}

void UEnemyAttackNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// Reset for next use
	bDamageApplied = false;
}

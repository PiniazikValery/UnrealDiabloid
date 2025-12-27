// EnemyAttackNotifyState.h
// Notify state for enemy attack animations that triggers damage via EnemyDamageComponent

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "EnemyAttackNotifyState.generated.h"

/**
 * Animation notify state for enemy attacks.
 * When this notify fires during an enemy attack montage, it applies pending damage
 * stored in the EnemyDamageComponent attached to the pooled skeletal mesh actor.
 *
 * Usage:
 * 1. Add this notify state to the enemy attack montage
 * 2. Position it at the point in the animation where the hit should land
 * 3. The EnemyVisualizationProcessor sets up pending damage when attack starts
 * 4. This notify applies the damage at the right moment
 */
UCLASS()
class MYPROJECT_API UEnemyAttackNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// Whether damage has been applied during this notify instance
	bool bDamageApplied = false;
};

// EnemyDamageComponent.h
// Component attached to pooled enemy skeletal mesh actors to enable animation-synced damage

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyDamageComponent.generated.h"

/**
 * Component that stores pending damage information for enemy attacks.
 * Attached to pooled skeletal mesh actors used by EnemyVisualizationProcessor.
 * Works with UEnemyAttackNotifyState to apply damage during attack animations.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UEnemyDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyDamageComponent();

	// Set up pending damage for an attack
	void SetPendingDamage(AActor* InTargetActor, float InDamage, const FVector& InAttackerLocation);

	// Apply the pending damage (called by notify state)
	// Returns true if damage was applied
	bool ApplyPendingDamage();

	// Clear pending damage without applying
	void ClearPendingDamage();

	// Check if there's pending damage
	bool HasPendingDamage() const { return bHasPendingDamage; }

protected:
	// The actor to damage
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	// Damage amount
	float PendingDamage = 0.0f;

	// Location of attacker (for damage direction)
	FVector AttackerLocation = FVector::ZeroVector;

	// Is there pending damage?
	bool bHasPendingDamage = false;
};

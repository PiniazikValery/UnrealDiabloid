// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyAttackProcessor.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

UEnemyAttackProcessor::UEnemyAttackProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UEnemyAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FEnemyTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FEnemyAttackFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FEnemyStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FEnemyTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FEnemyDeadTag>(EMassFragmentPresence::None);
}

void UEnemyAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	UWorld*		World = EntityManager.GetWorld();

	if (EntityQuery.GetEntityManager() != nullptr)
	{
		EntityQuery.ForEachEntityChunk(Context,
			[this, DeltaTime, World](FMassExecutionContext& Context) {
				const auto TransformList = Context.GetFragmentView<FTransformFragment>();
				const auto TargetList = Context.GetFragmentView<FEnemyTargetFragment>();
				const auto AttackList = Context.GetMutableFragmentView<FEnemyAttackFragment>();
				const auto StateList = Context.GetFragmentView<FEnemyStateFragment>();

				const int32 NumEntities = Context.GetNumEntities();

				for (int32 i = 0; i < NumEntities; ++i)
				{
					// Skip dead enemies
					if (!StateList[i].bIsAlive)
					{
						continue;
					}

					const FVector				EnemyLocation = TransformList[i].GetTransform().GetLocation();
					const FEnemyTargetFragment& Target = TargetList[i];
					FEnemyAttackFragment&		Attack = AttackList[i];

					const float DistanceToTarget = Target.DistanceToTarget;

					// Attack range check (equivalent to your "if (DistanceToPlayer < 150.0f)")
					if (DistanceToTarget < Attack.AttackRange)
					{
						if (!Attack.bIsInAttackRange)
						{
							// Just entered attack range - immediate first attack
							Attack.bIsInAttackRange = true;
							Attack.TimeSinceLastAttack = 0.0f;

							// Execute attack
							ExecuteAttack(EnemyLocation, Target.TargetLocation,
								Attack.AttackDamage, Target.TargetActor.Get(), World);
						}

						// Cooldown timer for subsequent attacks
						Attack.TimeSinceLastAttack += DeltaTime;

						// Attack every 1.5 seconds (equivalent to your attack interval)
						if (Attack.TimeSinceLastAttack >= Attack.AttackInterval)
						{
							Attack.TimeSinceLastAttack = 0.0f;
							ExecuteAttack(EnemyLocation, Target.TargetLocation,
								Attack.AttackDamage, Target.TargetActor.Get(), World);
						}
					}
					else
					{
						// Exited attack range
						if (Attack.bIsInAttackRange)
						{
							Attack.bIsInAttackRange = false;
							Attack.TimeSinceLastAttack = 0.0f;
						}
					}
				}
			});
	}
}

void UEnemyAttackProcessor::ExecuteAttack(const FVector& AttackerLocation, const FVector& TargetLocation,
	float Damage, AActor* TargetActor, UWorld* World)
{
	if (TargetActor && TargetActor->CanBeDamaged())
	{
		// Apply damage to player (equivalent to calling your PlayZombieAttack)
		// UGameplayStatics::ApplyDamage(
		// 	TargetActor,
		// 	Damage,
		// 	nullptr, // Instigator controller
		// 	nullptr, // Damage causer
		// 	UDamageType::StaticClass());

		// TODO: Trigger attack animation on visualization
		// TODO: Play attack sound
		// TODO: Spawn attack VFX using Niagara
	}
}

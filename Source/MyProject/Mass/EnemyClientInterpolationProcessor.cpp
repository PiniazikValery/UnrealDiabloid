// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyClientInterpolationProcessor.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "Engine/World.h"

UEnemyClientInterpolationProcessor::UEnemyClientInterpolationProcessor()
	: EntityQuery(*this)
{
	// Run on CLIENT only
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client);

	// Run every frame during PrePhysics
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	bAutoRegisterWithProcessingPhases = true;
}

void UEnemyClientInterpolationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FEnemyNetworkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FEnemyStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FEnemyTag>(EMassFragmentPresence::All);
}

void UEnemyClientInterpolationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Only run on client (includes Listen Server client entities)
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || World->GetNetMode() == NM_Standalone)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// Stats tracking
	int32 TotalProcessed = 0;
	int32 SkippedNoNetworkID = 0;
	int32 SkippedNoFirstUpdate = 0;
	int32 Teleported = 0;

	EntityQuery.ForEachEntityChunk(Context, [&, this](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FEnemyNetworkFragment> Networks = Context.GetMutableFragmentView<FEnemyNetworkFragment>();
		const TConstArrayView<FEnemyStateFragment> States = Context.GetFragmentView<FEnemyStateFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FTransformFragment& Transform = Transforms[i];
			FEnemyNetworkFragment& Network = Networks[i];
			const FEnemyStateFragment& State = States[i];

			// Skip server-authoritative entities (NetworkID == INDEX_NONE means server-owned)
			// Only interpolate client shadow copies (NetworkID is assigned)
			if (Network.NetworkID == INDEX_NONE)
			{
				SkippedNoNetworkID++;
				continue;
			}

			// If no first update yet, skip (but log warning if entity is old)
			if (!Network.bHasReceivedFirstUpdate)
			{
				Network.TimeSinceLastUpdate += DeltaTime;

				// Warn if entity hasn't received update for too long (might be network issue)
				if (Network.TimeSinceLastUpdate > 2.0f)
				{
					UE_LOG(LogTemp, Warning, TEXT("[INTERPOLATION] Entity NetworkID %d stuck - no updates for %.2fs"),
						Network.NetworkID, Network.TimeSinceLastUpdate);
				}
				SkippedNoFirstUpdate++;
				continue;
			}

			TotalProcessed++;

			// Update time tracking
			Network.TimeSinceLastUpdate += DeltaTime;

			// Calculate distance to target
			FVector CurrentPos = Transform.GetTransform().GetLocation();
			float DistanceToTarget = FVector::Dist(CurrentPos, Network.TargetPosition);

			// Teleport if too far (entity probably respawned or major desync)
			if (DistanceToTarget > TeleportThreshold)
			{
				Transform.SetTransform(FTransform(
					FRotator(0.0f, Network.TargetYaw, 0.0f),
					Network.TargetPosition,
					FVector::OneVector
				));
				Network.InterpolationAlpha = 1.0f;
				Teleported++;
				continue;
			}

			// Update interpolation alpha
			float InterpolationDuration = Network.ExpectedUpdateInterval;
			if (InterpolationDuration > 0.001f)
			{
				Network.InterpolationAlpha += DeltaTime / InterpolationDuration;
			}
			else
			{
				Network.InterpolationAlpha = 1.0f;
			}

			FVector NewPosition;
			float NewYaw;

			if (Network.InterpolationAlpha < 1.0f)
			{
				// INTERPOLATION: Smoothly move from previous to target
				float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Network.InterpolationAlpha);

				NewPosition = FMath::Lerp(Network.PreviousPosition, Network.TargetPosition, SmoothAlpha);
				NewYaw = FMath::Lerp(Network.PreviousYaw, Network.TargetYaw, SmoothAlpha);
			}
			else
			{
				// EXTRAPOLATION: Predict beyond target using velocity
				float ExtrapolationTime = FMath::Min(
					Network.TimeSinceLastUpdate - Network.ExpectedUpdateInterval,
					MaxExtrapolationTime
				);

				if (ExtrapolationTime > 0.0f && State.bIsMoving)
				{
					// Predict position using velocity
					NewPosition = Network.TargetPosition + (Network.TargetVelocity * ExtrapolationTime);
					NewYaw = Network.TargetYaw;
				}
				else
				{
					// At target, no extrapolation
					NewPosition = Network.TargetPosition;
					NewYaw = Network.TargetYaw;
				}
			}

			// Apply new transform
			Transform.SetTransform(FTransform(
				FRotator(0.0f, NewYaw, 0.0f),
				NewPosition,
				FVector::OneVector
			));
		}
	});

	// Log stats every second (using static timer)
	static float LogTimer = 0.0f;
	LogTimer += DeltaTime;
	if (LogTimer >= 1.0f)
	{
		LogTimer = 0.0f;
		UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION-LAG] Processed: %d | Skipped (NoNetID): %d | Skipped (NoUpdate): %d | Teleported: %d"),
			TotalProcessed, SkippedNoNetworkID, SkippedNoFirstUpdate, Teleported);
	}
}

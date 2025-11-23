// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoAimHelper.h"
#include "EnemyCharacter.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

UAutoAimHelper::UAutoAimHelper()
{
}

FAutoAimResult UAutoAimHelper::FindBestTargetAndAngle(
	AActor* SourceActor,
	TSubclassOf<AActor> TargetClass,
	float SearchRange,
	float MaxAngleDegrees,
	ETargetSelectionMode SelectionMode)
{
	FAutoAimResult Result;
	
	if (!SourceActor || !SourceActor->GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoAimHelper: Invalid SourceActor"));
		return Result;
	}
	
	// Default to AEnemyCharacter if no class specified
	if (!TargetClass)
	{
		TargetClass = AEnemyCharacter::StaticClass();
	}
	
	// Find all potential targets
	TArray<AActor*> PotentialTargets = FindTargetsInArc(
		SourceActor,
		TargetClass,
		SearchRange,
		MaxAngleDegrees
	);
	
	if (PotentialTargets.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("AutoAimHelper: No targets found in range"));
		return Result;
	}
	
	// Find best target based on selection mode
	AActor* BestTarget = nullptr;
	float BestScore = FLT_MAX;
	float BestAngle = 0.f;
	
	const FVector SourceLocation = SourceActor->GetActorLocation();
	
	UE_LOG(LogTemp, Warning, TEXT("AutoAimHelper: Evaluating %d potential targets"), PotentialTargets.Num());
	
	for (AActor* Target : PotentialTargets)
	{
		float AngleToTarget = 0.f;
		const FVector TargetLocation = Target->GetActorLocation();
		
		// Calculate angle BEFORE arc check so we can see it
		AngleToTarget = CalculateAimAngleToTarget(SourceActor, Target);
		
		// Verify target is in arc (should be, but double-check)
		float ArcCheckAngle = 0.f;
		if (!IsTargetInFrontArc(SourceActor, TargetLocation, MaxAngleDegrees, ArcCheckAngle))
		{
			UE_LOG(LogTemp, Warning, TEXT("Target %s rejected - outside arc"), *Target->GetName());
			continue;
		}
		
		// Calculate score
		float Score = CalculateTargetScore(SourceActor, Target, AngleToTarget, SelectionMode);
		
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Target;
			BestAngle = AngleToTarget;
		}
	}
	
	// Fill result
	if (BestTarget)
	{
		Result.Target = BestTarget;
		Result.AimAngle = BestAngle;
		Result.DistanceToTarget = FVector::Dist(SourceLocation, BestTarget->GetActorLocation());
		Result.bTargetFound = true;
		
		UE_LOG(LogTemp, Log, TEXT("AutoAimHelper: Best target found at angle %f, distance %f"),
			Result.AimAngle, Result.DistanceToTarget);
	}
	
	return Result;
}

float UAutoAimHelper::CalculateAimAngleToTarget(AActor* SourceActor, AActor* Target)
{
	if (!SourceActor || !Target)
	{
		return 0.f;
	}
	
	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector SourceForward = SourceActor->GetActorForwardVector();
	const FRotator SourceRotation = SourceActor->GetActorRotation();
	const FVector TargetLocation = Target->GetActorLocation();
	
	// Calculate direction to target (2D only, ignore Z)
	FVector DirectionToTarget = TargetLocation - SourceLocation;
	DirectionToTarget.Z = 0.f;
	DirectionToTarget.Normalize();
	
	// Calculate raw angles
	float TargetAtan2 = FMath::Atan2(DirectionToTarget.Y, DirectionToTarget.X);
	float ForwardAtan2 = FMath::Atan2(SourceForward.Y, SourceForward.X);
	float RawAngleDegrees = FMath::RadiansToDegrees(TargetAtan2 - ForwardAtan2);
	
	// Debug logging
	UE_LOG(LogTemp, Warning, TEXT("===== AUTO-AIM ANGLE CALCULATION ====="));
	UE_LOG(LogTemp, Warning, TEXT("Source: %s"), *SourceActor->GetName());
	UE_LOG(LogTemp, Warning, TEXT("Target: %s"), *Target->GetName());
	UE_LOG(LogTemp, Warning, TEXT("SourceLocation: %s"), *SourceLocation.ToString());
	UE_LOG(LogTemp, Warning, TEXT("TargetLocation: %s"), *TargetLocation.ToString());
	UE_LOG(LogTemp, Warning, TEXT("SourceRotation: %s"), *SourceRotation.ToString());
	UE_LOG(LogTemp, Warning, TEXT("SourceForward: %s"), *SourceForward.ToString());
	UE_LOG(LogTemp, Warning, TEXT("DirectionToTarget: %s"), *DirectionToTarget.ToString());
	UE_LOG(LogTemp, Warning, TEXT("Target Atan2: %f rad (%f deg)"), TargetAtan2, FMath::RadiansToDegrees(TargetAtan2));
	UE_LOG(LogTemp, Warning, TEXT("Forward Atan2: %f rad (%f deg)"), ForwardAtan2, FMath::RadiansToDegrees(ForwardAtan2));
	UE_LOG(LogTemp, Warning, TEXT("Raw Angle (before normalization): %f"), RawAngleDegrees);
	
	// Normalize to -180 to 180 range
	float AngleDegrees = RawAngleDegrees;
	while (AngleDegrees > 180.f) AngleDegrees -= 360.f;
	while (AngleDegrees < -180.f) AngleDegrees += 360.f;
	
	UE_LOG(LogTemp, Warning, TEXT("Normalized Angle: %f"), AngleDegrees);
	
	// Mirror the angle by negating it
	float MirroredAngle = -AngleDegrees;
	
	UE_LOG(LogTemp, Warning, TEXT("Angle before mirroring: %f"), AngleDegrees);
	UE_LOG(LogTemp, Warning, TEXT("Angle after mirroring: %f"), MirroredAngle);
	UE_LOG(LogTemp, Warning, TEXT("======================================"));
	
	// Return mirrored angle
	return MirroredAngle;
}

bool UAutoAimHelper::IsTargetInFrontArc(
	AActor* SourceActor,
	FVector TargetLocation,
	float MaxAngleDegrees,
	float& OutAngle)
{
	if (!SourceActor)
	{
		return false;
	}
	
	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector SourceForward = SourceActor->GetActorForwardVector();
	
	// Calculate direction to target (2D)
	FVector DirectionToTarget = TargetLocation - SourceLocation;
	DirectionToTarget.Z = 0.f;
	
	if (DirectionToTarget.IsNearlyZero())
	{
		return false;
	}
	
	DirectionToTarget.Normalize();
	
	// Calculate signed angle using atan2
	float AngleDegrees = FMath::RadiansToDegrees(
		FMath::Atan2(DirectionToTarget.Y, DirectionToTarget.X) - 
		FMath::Atan2(SourceForward.Y, SourceForward.X)
	);
	
	// Normalize to -180 to 180 range
	while (AngleDegrees > 180.f) AngleDegrees -= 360.f;
	while (AngleDegrees < -180.f) AngleDegrees += 360.f;
	
	// Check if within arc (absolute value)
	if (FMath::Abs(AngleDegrees) <= MaxAngleDegrees)
	{
		OutAngle = AngleDegrees;
		return true;
	}
	
	return false;
}

TArray<AActor*> UAutoAimHelper::FindTargetsInArc(
	AActor* SourceActor,
	TSubclassOf<AActor> TargetClass,
	float SearchRange,
	float MaxAngleDegrees)
{
	TArray<AActor*> ValidTargets;
	
	if (!SourceActor || !SourceActor->GetWorld())
	{
		return ValidTargets;
	}
	
	const FVector SourceLocation = SourceActor->GetActorLocation();
	
	// Setup sphere overlap
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SearchRange);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(SourceActor);
	
	// Find all overlapping actors
	TArray<FOverlapResult> OverlapResults;
	SourceActor->GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		SourceLocation,
		FQuat::Identity,
		ECC_Pawn,
		SphereShape,
		QueryParams
	);
	
	// Filter by class and arc
	for (const FOverlapResult& Overlap : OverlapResults)
	{
		AActor* Actor = Overlap.GetActor();
		
		// Check class
		if (!Actor || !Actor->IsA(TargetClass))
		{
			continue;
		}
		
		// Check if in front arc
		float AngleToTarget = 0.f;
		if (IsTargetInFrontArc(SourceActor, Actor->GetActorLocation(), MaxAngleDegrees, AngleToTarget))
		{
			ValidTargets.Add(Actor);
		}
	}
	
	return ValidTargets;
}

float UAutoAimHelper::CalculateTargetScore(
	AActor* SourceActor,
	AActor* Target,
	float AngleToTarget,
	ETargetSelectionMode SelectionMode)
{
	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist(SourceLocation, TargetLocation);
	
	switch (SelectionMode)
	{
		case ETargetSelectionMode::ClosestToCenter:
			// Lower angle = better (closer to center)
			return FMath::Abs(AngleToTarget);
			
		case ETargetSelectionMode::ClosestByDistance:
			// Lower distance = better
			return Distance;
			
		case ETargetSelectionMode::LowestHealth:
		{
			// Try to get health from target
			// You'll need to implement IHealthInterface or similar
			// For now, fallback to distance
			return Distance;
		}
			
		case ETargetSelectionMode::HighestThreat:
		{
			// Combine factors: closer + more centered = higher threat
			// Normalize both factors and combine
			const float NormalizedAngle = FMath::Abs(AngleToTarget) / 90.f; // 0-1
			const float NormalizedDistance = Distance / 1000.f; // 0-1 (assuming max 1000 range)
			return (NormalizedAngle + NormalizedDistance) * 0.5f;
		}
			
		default:
			return Distance;
	}
}

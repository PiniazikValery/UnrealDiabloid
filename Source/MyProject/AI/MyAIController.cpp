// Fill out your copyright notice in the Description page of Project Settings.

#include "MyAIController.h"

AMyAIController::AMyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	MoveDelay = 0.1f;
}

void AMyAIController::BeginPlay()
{
	Super::BeginPlay();

	UCrowdFollowingComponent* CrowdFollowingComponent = FindComponentByClass<UCrowdFollowingComponent>();
	if (CrowdFollowingComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("CrowdFollowingComponent found"));
		CrowdFollowingComponent->SetCrowdSeparation(true);
		CrowdFollowingComponent->SetCrowdSeparationWeight(10.0f);
		CrowdFollowingComponent->SetCrowdAvoidanceRangeMultiplier(1.f);
		CrowdFollowingComponent->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
	}

	// Get reference to the player pawn
	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn)
	{
		// Start following the player
		FollowPlayer();
	}
}

void AMyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("AI finished move"));
	Super::OnMoveCompleted(RequestID, Result);
	AMyProjectCharacter* ControlledCharacter = Cast<AMyProjectCharacter>(GetPawn());
	if (ControlledCharacter)
	{
		ControlledCharacter->SetIsPlayerTryingToMove(false);
	}
}

void AMyAIController::FollowPlayer()
{
	if (PlayerPawn)
	{
		UE_LOG(LogTemp, Log, TEXT("AI begin follow"));
		// Schedule the first call to MoveToPlayer
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_MoveToActor, this, &AMyAIController::MoveToPlayer, MoveDelay, true, 0);
	}
}

void AMyAIController::MoveToPlayer()
{
	if (PlayerPawn)
	{
		FVector PlayerLocation = PlayerPawn->GetActorLocation();
		FVector AILocation = GetPawn()->GetActorLocation();

		// Calculate the distance between the AI and the player
		float DistanceToPlayer = FVector::Dist(AILocation, PlayerLocation);

		// Move to the player if the distance is greater than the acceptance radius
		AMyProjectCharacter* ControlledCharacter = Cast<AMyProjectCharacter>(GetPawn());
		if (ControlledCharacter && DistanceToPlayer > 200)
		{
			UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
			FNavLocation		 ClosestReachablePoint;
			if (NavSys)
			{
				bool bProjected = NavSys->ProjectPointToNavigation(
					PlayerLocation,
					ClosestReachablePoint,
					FVector(100, 100, 100) // Define search area
				);
				if (bProjected)
				{
					UNavigationPath* NavPath = NavSys->FindPathToLocationSynchronously(
						GetWorld(),
						GetPawn()->GetActorLocation(),
						ClosestReachablePoint.Location);
					if (NavPath && NavPath->IsValid())
					{
						UE_LOG(LogTemp, Log, TEXT("Path found"));
						// NavPath->PathPoints.Last();
						EPathFollowingRequestResult::Type MoveResult = MoveToLocation(NavPath->PathPoints.Last());
						UE_LOG(LogTemp, Log, TEXT("NPC follow Location: %s"), *NavPath->PathPoints.Last().ToString());
						UE_LOG(LogTemp, Log, TEXT("Player Location: %s"), *PlayerLocation.ToString());
						if (MoveResult == 0)
						{
							UE_LOG(LogTemp, Log, TEXT("MoveResult Failed"));
							// follow = true;
							ControlledCharacter->SetIsPlayerTryingToMove(false);
						}
						if (MoveResult == 1)
						{
							UE_LOG(LogTemp, Log, TEXT("MoveResult AlreadyAtGoal"));
							// follow = true;
							ControlledCharacter->SetIsPlayerTryingToMove(false);
						}
						if (MoveResult == 2)
						{
							UE_LOG(LogTemp, Log, TEXT("MoveResult RequestSuccessful"));
							ControlledCharacter->SetIsPlayerTryingToMove(true);
						}
					}
				}
			}
			// EPathFollowingRequestResult::Type MoveResult = MoveToActor(PlayerPawn, 50, true, true, true, nullptr, true);
			// if (MoveResult == 0)
			//{
			//	UE_LOG(LogTemp, Log, TEXT("MoveResult Failed"));
			//	// follow = true;
			//	ControlledCharacter->SetIsPlayerTryingToMove(false);
			// }
			// if (MoveResult == 1)
			//{
			//	UE_LOG(LogTemp, Log, TEXT("MoveResult AlreadyAtGoal"));
			//	// follow = true;
			//	ControlledCharacter->SetIsPlayerTryingToMove(false);
			// }
			// if (MoveResult == 2)
			//{
			//	UE_LOG(LogTemp, Log, TEXT("MoveResult RequestSuccessful"));
			//	ControlledCharacter->SetIsPlayerTryingToMove(true);
			// }
		}
	}
}

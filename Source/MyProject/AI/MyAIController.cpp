#include "MyAIController.h"

AMyAIController::AMyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMyAIController::BeginPlay()
{
	Super::BeginPlay();

	UCrowdFollowingComponent* CrowdFollowingComponent = FindComponentByClass<UCrowdFollowingComponent>();
	if (CrowdFollowingComponent)
	{
		CrowdFollowingComponent->SetCrowdCollisionQueryRange(300);
		CrowdFollowingComponent->SetCrowdSeparation(true);
		CrowdFollowingComponent->SetCrowdSeparationWeight(200.0f);
		CrowdFollowingComponent->SetCrowdAvoidanceRangeMultiplier(1.f);
		CrowdFollowingComponent->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
	}

	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn)
	{
		PreviousPlayerLocation = PlayerPawn->GetActorLocation();
	}
}

void AMyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Agent = Cast<AMyProjectCharacter>(InPawn);
	if (Agent)
	{
		UCharacterMovementComponent* Move = Agent->GetCharacterMovement();
		Move->MaxAcceleration = 2048.f;
		Move->BrakingDecelerationWalking = 2048.f;
		Move->BrakingFrictionFactor = 2.0f;
		Move->bRequestedMoveUseAcceleration = false;
		Move->bUseRVOAvoidance = true;
		Move->RotationRate = FRotator(0.0f, 250.0f, 0.0f);
		Move->bOrientRotationToMovement = true;
		//Move->bAllowStrafe = true;

		Agent->GetCapsuleComponent()->SetSimulatePhysics(false);
		Agent->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

		MoveToPlayer();
	}
}

void AMyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (Agent)
	{
		Agent->SetIsPlayerTryingToMove(false);
	}
}

void AMyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!PlayerPawn || !GetPawn())
		return;

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const FVector AILocation = GetPawn()->GetActorLocation();
	const float	  DistanceToPlayer = FVector::Dist(PlayerLocation, AILocation);

	// Attack if in range
	if (DistanceToPlayer < 100.0f)
	{
		if (!bIsInAttackRange)
		{
			bIsInAttackRange = true;
			StopMovement();

			if (Agent)
			{
				Agent->SetIsPlayerTryingToMove(false);
				// Agent->StartAttack(); // Enable when ready
			}
		}
		return;
	}
	else if (bIsInAttackRange)
	{
		bIsInAttackRange = false;
	}

	TimeSinceLastMoveRequest += DeltaSeconds;

	// Only update path if player moved far enough or we're idle
	if (TimeSinceLastMoveRequest >= 0.5f)
	{
		TimeSinceLastMoveRequest = 0.f;

		const float PlayerMovedDistance = FVector::Dist(PlayerLocation, PreviousPlayerLocation);
		if (PlayerMovedDistance > 50.f)
		{
			MoveToPlayer();
			PreviousPlayerLocation = PlayerLocation;
		}
	}
}

void AMyAIController::MoveToPlayer()
{
	if (!PlayerPawn || !Agent)
		return;

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const float	  AcceptanceRadius = 30.0f;

	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(PlayerLocation, AcceptanceRadius);

	switch (MoveResult)
	{
		case EPathFollowingRequestResult::AlreadyAtGoal:
		case EPathFollowingRequestResult::Failed:
			Agent->SetIsPlayerTryingToMove(false);
			break;

		case EPathFollowingRequestResult::RequestSuccessful:
			Agent->SetIsPlayerTryingToMove(true);
			break;
	}

	LocationToMove = PlayerLocation;
}

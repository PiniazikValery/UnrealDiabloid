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

	// Face the player
	FVector Direction = PlayerLocation - AILocation;
	Direction.Z = 0;
	if (!Direction.IsNearlyZero())
	{
		FRotator NewRotation = Direction.Rotation();
		GetPawn()->SetActorRotation(FMath::RInterpTo(GetPawn()->GetActorRotation(), NewRotation, DeltaSeconds, 10.0f));
	}

	// Attack if in range
	if (DistanceToPlayer < 150.0f)
	{
		if (!bIsInAttackRange)
		{
			bIsInAttackRange = true;
			// Immediate first attack
			PerformAttack();
			TimeSinceLastAttack = 0.f;
		}

		// Perform attack every 1.5 seconds
		TimeSinceLastAttack += DeltaSeconds;
		if (TimeSinceLastAttack >= 1.5f)
		{
			PerformAttack();
			TimeSinceLastAttack = 0.f;
		}
	}
	else if (bIsInAttackRange)
	{
		bIsInAttackRange = false;
		TimeSinceLastAttack = 0.f;
	}

	// Always keep moving toward player
	TimeSinceLastMoveRequest += DeltaSeconds;

	// Update path frequently to follow player smoothly
	if (TimeSinceLastMoveRequest >= 0.2f)
	{
		TimeSinceLastMoveRequest = 0.f;

		const float PlayerMovedDistance = FVector::Dist(PlayerLocation, PreviousPlayerLocation);
		// Move even if player hasn't moved much, to continuously chase
		if (PlayerMovedDistance > 10.f || DistanceToPlayer > 50.0f)
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
	const float	  AcceptanceRadius = 30.0f; // Smaller radius to get closer

	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(PlayerLocation, AcceptanceRadius);

	switch (MoveResult)
	{
		case EPathFollowingRequestResult::AlreadyAtGoal:
			// Even at goal, keep trying to move for smooth following
			Agent->SetIsPlayerTryingToMove(true);
			break;
		case EPathFollowingRequestResult::Failed:
			Agent->SetIsPlayerTryingToMove(false);
			break;

		case EPathFollowingRequestResult::RequestSuccessful:
			Agent->SetIsPlayerTryingToMove(true);
			break;
	}

	LocationToMove = PlayerLocation;
}

void AMyAIController::PerformAttack()
{
	if (!Agent)
		return;

	// Cast to EnemyCharacter to use zombie attack
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Agent);
	if (Enemy)
	{
		Enemy->PlayZombieAttack();
		UE_LOG(LogTemp, Log, TEXT("Enemy performing zombie attack"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Agent is not an EnemyCharacter"));
	}
}

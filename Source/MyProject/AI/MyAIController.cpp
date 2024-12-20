// Fill out your copyright notice in the Description page of Project Settings.

#include "MyAIController.h"

AMyAIController::AMyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
}

void AMyAIController::BeginPlay()
{
	Super::BeginPlay();
	UCrowdFollowingComponent* CrowdFollowingComponent = FindComponentByClass<UCrowdFollowingComponent>();
	if (CrowdFollowingComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("CrowdFollowingComponent found"));
		CrowdFollowingComponent->SetCrowdCollisionQueryRange(300);
		CrowdFollowingComponent->SetCrowdSeparation(true);
		CrowdFollowingComponent->SetCrowdSeparationWeight(100.0f);
		CrowdFollowingComponent->SetCrowdAvoidanceRangeMultiplier(1.f);
		CrowdFollowingComponent->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
	}

	// Get reference to the player pawn
	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

void AMyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Agent = Cast<AMyProjectCharacter>(InPawn);
	if (Agent)
	{
		Agent->SwitchToRunning();
		Agent->GetCharacterMovement()->MaxAcceleration = 200048.0f;
		Agent->GetCharacterMovement()->bRequestedMoveUseAcceleration = false;
		Agent->GetCharacterMovement()->RotationRate = FRotator(0.0f, 100.0f, 0.0f);
		Agent->GetCharacterMovement()->MaxWalkSpeed = 200.0f; // Set the desired speed
		Agent->GetCharacterMovement()->MinAnalogWalkSpeed = 150.0f; // Optional: Minimum speed for analog input
		Agent->GetCharacterMovement()->bUseRVOAvoidance = true;
		Agent->GetCharacterMovement()->AvoidanceConsiderationRadius = 42.0f;
		Agent->GetCharacterMovement()->AvoidanceWeight = 0.6f;
		Agent->GetCapsuleComponent()->SetSimulatePhysics(false);
		Agent->GetCapsuleComponent()->SetEnableGravity(false);
		Agent->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		MoveToPlayer();
	}
}

void AMyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("AI finished move"));
	Super::OnMoveCompleted(RequestID, Result);
	AMyProjectCharacter* ControlledCharacter = Cast<AMyProjectCharacter>(GetPawn());
	if (ControlledCharacter)
	{
		follow = false;
		ControlledCharacter->SetIsPlayerTryingToMove(false);
	}
}

void AMyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	FVector AILocation = GetPawn()->GetActorLocation();

	if (!PlayerPawn)
		return;
	FVector CurrentPlayerLocation = PlayerPawn->GetActorLocation();
	if (bIsInitialTick)
	{
		PreviousPlayerLocation = CurrentPlayerLocation;
	}
	float	DistancePlayerMoved = FVector::Dist(PreviousPlayerLocation, CurrentPlayerLocation);
	FVector CurrentAgentLocation = GetPawn()->GetActorLocation();
	float	DistanceAgentMoved = FVector::Dist(PreviousAgentLocation, CurrentAgentLocation);

	PreviousAgentLocation = CurrentAgentLocation;

	float				 DistanceToPlayer = FVector::Dist(AILocation, PlayerLocation);
	AMyProjectCharacter* ControlledCharacter = Cast<AMyProjectCharacter>(GetPawn());
	if (ControlledCharacter)
	{
		if (follow && DistanceAgentMoved == 0)
		{
			StopMovement();
		}
		if (DistanceToPlayer > 500)
		{
			ControlledCharacter->SwitchToRunning();
		}
		else
		{
			ControlledCharacter->SwitchToWalking();
		}
	}
	if (DistancePlayerMoved > 0 && !follow)
	{
		PreviousPlayerLocation = CurrentPlayerLocation; // Update the previous location
		MoveToPlayer();
	}
	UCrowdFollowingComponent* CrowdComponent = FindComponentByClass<UCrowdFollowingComponent>();
	if (CrowdComponent)
	{
		const FVector AiDirection = CrowdComponent->GetCurrentDirection();

		if (ControlledCharacter)
		{
			ControlledCharacter->SetMovementVector(FVector2D(AiDirection.Y, AiDirection.X));
		}
	}
	if (bIsInitialTick)
	{
		bIsInitialTick = false;
	}
}
void AMyAIController::MoveToPlayer()
{
	if (!PlayerPawn)
		return;

	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	FVector AILocation = GetPawn()->GetActorLocation();

	float				 DistanceToPlayer = FVector::Dist(AILocation, PlayerLocation);

	EPathFollowingRequestResult::Type MoveResult = MoveToLocation(PlayerLocation, 50);

	if (MoveResult == 0)
	{
		follow = false;
		Agent->SetIsPlayerTryingToMove(false);
	}
	if (MoveResult == 1)
	{
		follow = false;
		Agent->SetIsPlayerTryingToMove(false);
	}
	if (MoveResult == 2)
	{
		follow = true;
		Agent->SetIsPlayerTryingToMove(true);
	}
}

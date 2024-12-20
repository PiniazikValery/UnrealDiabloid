// Fill out your copyright notice in the Description page of Project Settings.

#include "./CharacterInput.h"
#include "../MyProjectCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void UCharacterInput::onTriggeredMove(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("onTriggeredMove fired"));
	MyCharacter->SetIsPlayerTryingToMove(true);
	FVector2D MovementVector(Value.Get<FVector2D>().GetRotated(0.f));
	MyCharacter->SetMovementVector(MovementVector);

	if (MyCharacter->Controller != nullptr)
	{
		// Move in world space
		const FVector ForwardDirection = FVector::ForwardVector;
		const FVector RightDirection = FVector::RightVector;

		// Add movement input
		MyCharacter->AddMovementInput(ForwardDirection, MovementVector.Y);
		MyCharacter->AddMovementInput(RightDirection, MovementVector.X);
	}
}

void UCharacterInput::onOngoingMove()
{
	MyCharacter->SetIsPlayerTryingToMove(true);
}

void UCharacterInput::onNoneMove()
{
	MyCharacter->SetIsPlayerTryingToMove(false);
}

// Sets default values
UCharacterInput::UCharacterInput()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveInputActionAsset(TEXT("/Script/EnhancedInput.InputAction'/Game/ThirdPerson/Input/Actions/IA_Move.IA_Move'"));
	if (MoveInputActionAsset.Succeeded())
	{
		MoveAction = MoveInputActionAsset.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> ContextFinder(TEXT("/Script/EnhancedInput.InputMappingContext'/Game/ThirdPerson/Input/IMC_Default.IMC_Default'"));
	if (ContextFinder.Succeeded())
	{
		DefaultMappingContext = ContextFinder.Object;
	}
}

// Called when the game starts or when spawned
void UCharacterInput::BeginPlay()
{
	Super::BeginPlay();
	MyCharacter = Cast<AMyProjectCharacter>(GetOwner());
}

void UCharacterInput::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCharacterInput::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent, AController* Controller)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UCharacterInput::onTriggeredMove);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Ongoing, this, &UCharacterInput::onOngoingMove);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::None, this, &UCharacterInput::onNoneMove);
	}
}

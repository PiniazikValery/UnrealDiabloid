// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

//////////////////////////////////////////////////////////////////////////
// AMyProjectCharacter

AMyProjectCharacter::AMyProjectCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple'"));
	if (MeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshAsset.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, 270.0f, 0.0f));
	}
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), TEXT("weapon_r"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WeaponMeshAsset(TEXT("/Script/Engine.StaticMesh'/Game/Characters/Weapons/Staff/Staff.Staff'"));
	if (WeaponMeshAsset.Succeeded())
	{
		WeaponMesh->SetStaticMesh(WeaponMeshAsset.Object);
		// Disable collision for the weapon mesh
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPClass(TEXT("/Script/Engine.AnimBlueprint'/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C'"));
	if (AnimBPClass.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);
	}
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> ContextFinder(TEXT("/Script/EnhancedInput.InputMappingContext'/Game/ThirdPerson/Input/IMC_Default.IMC_Default'"));
	if (ContextFinder.Succeeded())
	{
		DefaultMappingContext = ContextFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveInputActionAsset(TEXT("/Script/EnhancedInput.InputAction'/Game/ThirdPerson/Input/Actions/IA_Move.IA_Move'"));
	if (MoveInputActionAsset.Succeeded())
	{
		MoveAction = MoveInputActionAsset.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> JumpInputActionAsset(TEXT("/Script/EnhancedInput.InputAction'/Game/ThirdPerson/Input/Actions/IA_Jump.IA_Jump'"));
	if (JumpInputActionAsset.Succeeded())
	{
		JumpAction = JumpInputActionAsset.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> RollInputActionAsset(TEXT("/Script/EnhancedInput.InputAction'/Game/ThirdPerson/Input/Actions/IA_Roll.IA_Roll'"));
	if (RollInputActionAsset.Succeeded())
	{
		RollAction = RollInputActionAsset.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimMontage> DodgeMontageAsset(TEXT("/Script/Engine.AnimMontage'/Game/Characters/Mannequins/Animations/Locomotion/Dodge/Dodge_Montage.Dodge_Montage'"));
	if (DodgeMontageAsset.Succeeded())
	{
		DodgeMontage = DodgeMontageAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontageAsset(TEXT("/Script/Engine.AnimMontage'/Game/Characters/Mannequins/Animations/Attack/Attack_Montage.Attack_Montage'"));
	if (AttackMontageAsset.Succeeded())
	{

		FirstAttackMontage = AttackMontageAsset.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimMontage> SecondAttackMontageAsset(TEXT("/Script/Engine.AnimMontage'/Game/Characters/Mannequins/Animations/Attack/Second_Attack_Montage.Second_Attack_Montage'"));
	if (SecondAttackMontageAsset.Succeeded())
	{

		SecondAttackMontage = SecondAttackMontageAsset.Object;
	}
	ProjectileSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(RootComponent);
	ProjectileSpawnPoint->SetRelativeLocation(FVector(100.0f, 0.0f, 50.0f));
	ProjectileSpawnPoint->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	ProjectileSpawnPoint->bHiddenInGame = false;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...
	GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 200.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->GroundFriction = 0.1;
	GetCharacterMovement()->BrakingDecelerationWalking = 1000;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	// GetCharacterMovement()->UseAccelerationForPathFollowing();
	// GetCharacterMovement()->bUseAccelerationForPaths = true;
	bUseControllerRotationYaw = false;

	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	CameraRoot->SetupAttachment(RootComponent);
	CameraRoot->SetUsingAbsoluteRotation(true);

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CameraRoot);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->SetRelativeRotation(FRotator(-45.f, 0.f, 0.f));

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	DoNothingDelegate.BindUObject(this, &AMyProjectCharacter::DoNothing);

	ProjectileClass = AMageProjectile::StaticClass();
}

void AMyProjectCharacter::InitializeCharacter(bool _isAI, UClass* _AIControllerClass)
{
	isAI = _isAI;
	if (_isAI)
	{
		SwitchToWalking();
		/*GetCapsuleComponent()->SetCollisionProfileName(TEXT("Custom"));
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);*/
		GetCharacterMovement()->MaxAcceleration = 200048.0f;
		/*GetCharacterMovement()->JumpZVelocity = 0.0f;
		GetCapsuleComponent()->SetSimulatePhysics(false);
		GetCapsuleComponent()->BodyInstance.bLockXTranslation = true;
		GetCapsuleComponent()->BodyInstance.bLockYTranslation = true;
		GetCapsuleComponent()->BodyInstance.bLockZTranslation = true;
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);*/
		AIControllerClass = _AIControllerClass;
		if (AIControllerClass)
		{
			AAIController* AIController = Cast<AAIController>(GetController());
			if (!AIController)
			{
				AIController = GetWorld()->SpawnActor<AAIController>(AIControllerClass);
				if (AIController)
				{
					AIController->Possess(this);
				}
			}
		}
	}
}

bool AMyProjectCharacter::GetIsPlayerTryingToMove()
{
	return IsPlayerTryingToMove;
}

void AMyProjectCharacter::SetIsPlayerTryingToMove(bool value)
{
	IsPlayerTryingToMove = value;
}

void AMyProjectCharacter::SetAllowPhysicsRotationDuringAnimRootMotion(bool value)
{
	GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = value;
}

void AMyProjectCharacter::SetOrientRotationToMovement(bool value)
{
	GetCharacterMovement()->bOrientRotationToMovement = value;
}

void AMyProjectCharacter::SetRotationRate(FRotator rotation)
{
	GetCharacterMovement()->RotationRate = rotation;
}

void AMyProjectCharacter::SmoothlyRotate(float degrees, float speed)
{
	SmoothRotationSpeed = speed;
	EndHorizontalSmoothRotationOffset = degrees;
	StartHorizontalSmoothRotationOffset = CurrentHorizontalSmoothRotationOffset;
	SmoothRotationElapsedTime = 0.f;
}

bool AMyProjectCharacter::GetIsWalking()
{
	return GetCharacterMovement()->MaxWalkSpeed <= 200;
}

float AMyProjectCharacter::GetInputDirection() const
{
	// If there's no input, return 0
	if (MovementVector.IsNearlyZero())
	{
		return 0.0f;
	}

	// Calculate the angle of the input in character's local space
	float InputAngle = FMath::RadiansToDegrees(FMath::Atan2(MovementVector.X, MovementVector.Y));

	// Normalize the angle to be between -180 and 180
	InputAngle = FMath::UnwindDegrees(InputAngle);

	return InputAngle;
}

bool AMyProjectCharacter::GetIsDodging() const
{
	return IsDodging;
}

void AMyProjectCharacter::SetIsInRollAnimation(bool value)
{
	if (IsDodging != value)
	{
		IsDodging = value;
	}
}

float AMyProjectCharacter::GetLookRotation()
{
	return CurrentHorizontalSmoothRotationOffset;
}

void AMyProjectCharacter::SetIsAttackEnding(bool value)
{
	IsAttackEnding = value;
}

void AMyProjectCharacter::SetIsSecondAttackWindowOpen(bool value)
{
	IsSecondAttackWindowOpen = value;
}

void AMyProjectCharacter::FireProjectile()
{
	if (ProjectileClass)
	{
		UE_LOG(LogTemp, Log, TEXT("Projectile class exists"));
		FVector	 SpawnLocation = ProjectileSpawnPoint->GetComponentLocation();
		FRotator SpawnRotation = ProjectileSpawnPoint->GetComponentRotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		AMageProjectile* Projectile = GetWorld()->SpawnActor<AMageProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);

		if (Projectile)
		{
			UE_LOG(LogTemp, Log, TEXT("Projectile spawned"));
			FVector LaunchDirection = SpawnRotation.Vector();
			Projectile->ProjectileMovement->Velocity = LaunchDirection * Projectile->ProjectileMovement->InitialSpeed;
		}
	}
}

// Input
void AMyProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//  Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Ongoing, this, &AMyProjectCharacter::OnMoving);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::None, this, &AMyProjectCharacter::OnIdle);

		PlayerInputComponent->BindTouch(IE_Pressed, this, &AMyProjectCharacter::OnSwipeStarted);
		PlayerInputComponent->BindTouch(IE_Repeat, this, &AMyProjectCharacter::OnSwipeUpdated);
		PlayerInputComponent->BindTouch(IE_Released, this, &AMyProjectCharacter::OnSwipeEnded);
	}
}

void AMyProjectCharacter::Tick(float DeltaTime)
{
	if (CurrentHorizontalSmoothRotationOffset != EndHorizontalSmoothRotationOffset)
	{
		SmoothRotationElapsedTime += DeltaTime * SmoothRotationSpeed;
		float Alpha = FMath::Clamp(SmoothRotationElapsedTime, 0.0f, 1.0f);
		CurrentHorizontalSmoothRotationOffset = FMath::Lerp(StartHorizontalSmoothRotationOffset, EndHorizontalSmoothRotationOffset, Alpha);
		ProjectileSpawnPoint->SetRelativeRotation(FRotator(0.0f, -CurrentHorizontalSmoothRotationOffset, 0.0f));
		float CurrentHorizontalSmoothRotationOffsetRad = ((CurrentHorizontalSmoothRotationOffset + 90) * 3.14) / 180;
		ProjectileSpawnPoint->SetRelativeLocation(FVector(100 * std::sin(CurrentHorizontalSmoothRotationOffsetRad), 100 * std::cos(CurrentHorizontalSmoothRotationOffsetRad), 50.0f));
	}

	float Velocity = GetCharacterMovement()->Velocity.Size();
	/*if (Velocity == 0)
	{
		IsPlayerTryingToMove = false;
	}*/
	// UE_LOG(LogTemp, Log, TEXT("newVelocity %f"), newVelocity);
	///*if (newVelocity >= previusVelocity) {
	//	IsPlayerTryingToMove =
	//}*/
	// IsPlayerTryingToMove = newVelocity != 0 && newVelocity >= previusVelocity;
	// previusVelocity = newVelocity;
	// IsPlayerTryingToMove = !Velocity.IsNearlyZero();
}

void AMyProjectCharacter::OnSwipeStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	SwipeStartLocation = Location;
	SwipePoints.Empty();
	SwipePoints.Add(Location);
	bIsSwipeInProgress = true;
}

void AMyProjectCharacter::OnSwipeUpdated(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (bIsSwipeInProgress)
	{
		SwipePoints.Add(Location);
	}
}

void AMyProjectCharacter::OnSwipeEnded(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (bIsSwipeInProgress)
	{
		SwipePoints.Add(Location);

		EGestureType DetectedGesture = Recognizer.RecognizeGesture(SwipePoints);

		switch (DetectedGesture)
		{
			case EGestureType::SwipeRight:
				UE_LOG(LogTemp, Log, TEXT("Swipe Right detected"));
				// Add your game logic for swipe right
				break;
			case EGestureType::SwipeLeft:
				UE_LOG(LogTemp, Log, TEXT("Swipe Left detected"));
				// Add your game logic for swipe left
				break;
			case EGestureType::SwipeUp:
				UE_LOG(LogTemp, Log, TEXT("Swipe Up detected"));
				break;
			case EGestureType::SwipeDown:
				UE_LOG(LogTemp, Log, TEXT("Swipe Down detected"));
				StartDodge();
				// Add your game logic for swipe down
				break;
			case EGestureType::Circle:
				UE_LOG(LogTemp, Log, TEXT("Circle gesture detected"));
				// Add your game logic for circle gesture
				break;
			case EGestureType::None:
				UE_LOG(LogTemp, Log, TEXT("No gesture detected"));
				std::random_device					  rd;
				std::mt19937						  gen(rd());
				std::uniform_real_distribution<float> dis(-90.0, 90.0);
				float								  randomFloat = dis(gen);

				SmoothlyRotate(randomFloat, 1);
				StartAttack();
				break;
		}

		bIsSwipeInProgress = false;
	}
}

void AMyProjectCharacter::StartAttack()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	SwitchToWalking();
	if (IsSecondAttackWindowOpen)
	{
		AnimInstance->Montage_SetEndDelegate(DoNothingDelegate, FirstAttackMontage);
		AnimInstance->Montage_SetEndDelegate(DoNothingDelegate, SecondAttackMontage);
		FinishAttackDelegate.Unbind();
		AnimInstance->Montage_Play(SecondAttackMontage);
		FinishAttackDelegate.BindUObject(this, &AMyProjectCharacter::FinishAttack);
		AnimInstance->Montage_SetEndDelegate(FinishAttackDelegate, SecondAttackMontage);
	}
	if (!IsAttacking || IsAttackEnding)
	{
		AnimInstance->Montage_SetEndDelegate(DoNothingDelegate, FirstAttackMontage);
		AnimInstance->Montage_SetEndDelegate(DoNothingDelegate, SecondAttackMontage);
		FinishAttackDelegate.Unbind();
		IsAttacking = true;
		AnimInstance->Montage_Play(FirstAttackMontage);
		FinishAttackDelegate.BindUObject(this, &AMyProjectCharacter::FinishAttack);
		AnimInstance->Montage_SetEndDelegate(FinishAttackDelegate, FirstAttackMontage);
	}
}

void AMyProjectCharacter::FinishAttack(UAnimMontage* Montage, bool bInterrupted)
{
	IsAttacking = false;
	UE_LOG(LogTemp, Warning, TEXT("Boolean value is: %s"), bInterrupted ? TEXT("true") : TEXT("false"));
	if (!bInterrupted)
	{
		SwitchToRunning();
	}
}

void AMyProjectCharacter::SwitchToWalking()
{
	GetCharacterMovement()->MaxWalkSpeed = 200.f;
}

void AMyProjectCharacter::SwitchToRunning()
{
	SmoothlyRotate(0, 10);
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
}

void AMyProjectCharacter::DoNothing(UAnimMontage* Montage, bool bInterrupted)
{
}

void AMyProjectCharacter::StartDodge()
{
	if (!IsDodging)
	{
		SwitchToRunning();
		GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = true;
		IsDodging = true;
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		AnimInstance->Montage_Play(DodgeMontage);
		FOnMontageEnded BlendingOutDelegate;
		BlendingOutDelegate.BindUObject(this, &AMyProjectCharacter::FinishDodge);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, DodgeMontage);
	}
}

void AMyProjectCharacter::FinishDodge(UAnimMontage* Montage, bool bInterrupted)
{
	IsDodging = false;
	GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = false;
}

void AMyProjectCharacter::Move(const FInputActionValue& Value)
{
	IsPlayerTryingToMove = true;
	//  input is a Vector2D
	MovementVector = Value.Get<FVector2D>();
	MovementVector = MovementVector.GetRotated(RollMovementRotation);

	if (Controller != nullptr)
	{
		// Move in world space
		const FVector ForwardDirection = FVector::ForwardVector;
		const FVector RightDirection = FVector::RightVector;

		// Add movement input
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMyProjectCharacter::OnMoving()
{
	IsPlayerTryingToMove = true;
}

void AMyProjectCharacter::OnIdle()
{
	IsPlayerTryingToMove = false;
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectCharacter.h"
#include "UMyGestureRecognizer.h"
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
#include "Net/UnrealNetwork.h"
#include <random>
#include <cmath>
#include "AIController.h"
#include "Components/RotationSmoothingComponent.h"
#include "Components/ProjectileSpawnerComponent.h"

AMyProjectCharacter::AMyProjectCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	InitializeMesh();
	InitializeWeapon();
	InitializeAnimations();
	InitializeInput();
	InitializeMovement();
	InitializeCamera();
	InitializeProjectileSpawnPoint(); // kept for backward compatibility (spawner component also creates one)

	bReplicates = true;
	InputHandler = CreateDefaultSubobject<UCharacterInput>(TEXT("InputHandler"));
	GestureRecognizer = CreateDefaultSubobject<UMyGestureRecognizer>(TEXT("GestureRecognizer"));
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	RotationSmoothingComponent = CreateDefaultSubobject<URotationSmoothingComponent>(TEXT("RotationSmoothingComponent"));
	ProjectileSpawnerComponent = CreateDefaultSubobject<UProjectileSpawnerComponent>(TEXT("ProjectileSpawnerComponent"));
	if (RotationSmoothingComponent && ProjectileSpawnerComponent)
	{
		RotationSmoothingComponent->OnRotationOffsetChanged.AddDynamic(this, &AMyProjectCharacter::HandleRotationOffsetChanged);
	}
	GestureRecognizer->OnGestureRecognized.AddDynamic(this, &AMyProjectCharacter::HandleGesture);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ProjectileClass = AMageProjectile::StaticClass();
}

void AMyProjectCharacter::InitializeMesh()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshAsset.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, 270.0f, 0.0f));
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
}

void AMyProjectCharacter::InitializeWeapon()
{
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), TEXT("weapon_r"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> WeaponMeshAsset(TEXT("/Game/Characters/Weapons/Staff/Staff.Staff"));
	if (WeaponMeshAsset.Succeeded())
	{
		WeaponMesh->SetStaticMesh(WeaponMeshAsset.Object);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
}

void AMyProjectCharacter::InitializeAnimations()
{
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPClass(TEXT("/Game/Characters/Mannequins/Animations/ABP.ABP_C"));
	if (AnimBPClass.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DodgeMontageAsset(TEXT("/Game/Characters/Mannequins/Animations/Locomotion/Dodge/Dodge_Montage.Dodge_Montage"));
	if (DodgeMontageAsset.Succeeded()) DodgeMontage = DodgeMontageAsset.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> StartFMontageAsset(TEXT("/Game/Characters/Mannequins/Animations/Locomotion/Start/Start_F_Montage.Start_F_Montage"));
	if (StartFMontageAsset.Succeeded()) StartFMontage = StartFMontageAsset.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> StartRMontageAsset(TEXT("/Game/Characters/Mannequins/Animations/Locomotion/Start/Start_R_Montage.Start_R_Montage"));
	if (StartRMontageAsset.Succeeded()) StartRMontage = StartRMontageAsset.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontageAsset(TEXT("/Game/Characters/Mannequins/Animations/Attack/Attack_Montage.Attack_Montage"));
	if (AttackMontageAsset.Succeeded()) FirstAttackMontage = AttackMontageAsset.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> SecondAttackMontageAsset(TEXT("/Game/Characters/Mannequins/Animations/Attack/Second_Attack_Montage.Second_Attack_Montage"));
	if (SecondAttackMontageAsset.Succeeded()) SecondAttackMontage = SecondAttackMontageAsset.Object;
}

void AMyProjectCharacter::InitializeInput()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> ContextFinder(TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default"));
	if (ContextFinder.Succeeded()) DefaultMappingContext = ContextFinder.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpInputActionAsset(TEXT("/Game/ThirdPerson/Input/Actions/IA_Jump.IA_Jump"));
	if (JumpInputActionAsset.Succeeded()) JumpAction = JumpInputActionAsset.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> RollInputActionAsset(TEXT("/Game/ThirdPerson/Input/Actions/IA_Roll.IA_Roll"));
	if (RollInputActionAsset.Succeeded()) RollAction = RollInputActionAsset.Object;
}

void AMyProjectCharacter::InitializeMovement()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
	MoveComp->RotationRate = FRotator(0.0f, 400.0f, 0.0f);
	MoveComp->JumpZVelocity = 700.f;
	MoveComp->AirControl = 0.35f;
	MoveComp->MaxWalkSpeed = 500.f;
	MoveComp->MinAnalogWalkSpeed = 20.f;
	MoveComp->GroundFriction = 0.1;
	MoveComp->BrakingDecelerationWalking = 1000;
	MoveComp->BrakingDecelerationFalling = 1500.0f;
	MoveComp->bUseFlatBaseForFloorChecks = true;
}

void AMyProjectCharacter::InitializeCamera()
{
	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	CameraRoot->SetupAttachment(RootComponent);
	CameraRoot->SetUsingAbsoluteRotation(true);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CameraRoot);
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->SetRelativeRotation(FRotator(-30.f, 0.f, 0.f));

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AMyProjectCharacter::InitializeProjectileSpawnPoint()
{
	ProjectileSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(RootComponent);
	ProjectileSpawnPoint->SetRelativeLocation(FVector(100.0f, 0.0f, 50.0f));
	ProjectileSpawnPoint->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	ProjectileSpawnPoint->bHiddenInGame = false;
}

void AMyProjectCharacter::PossessAIController(UClass* _AIControllerClass)
{
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

bool AMyProjectCharacter::GetIsPlayerTryingToMove()
{
	return IsPlayerTryingToMove;
}

void AMyProjectCharacter::SetIsPlayerTryingToMove(bool value)
{
	if (IsLocallyControlled())
	{
		ServerSetIsPlayerTryingToMove(value);
	}
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
	if (RotationSmoothingComponent)
	{
		RotationSmoothingComponent->SmoothlyRotate(degrees, speed);
	}
}

bool AMyProjectCharacter::GetWithoutRootStart()
{
	return withoutRootStart;
}

bool AMyProjectCharacter::GetIsWalking()
{
	return GetCharacterMovement()->Velocity.Size() <= 300;
}

float AMyProjectCharacter::GetInputDirection() const
{
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

bool AMyProjectCharacter::GetIsDodging() const { return CombatComponent ? CombatComponent->GetIsDodging() : false; }
bool AMyProjectCharacter::GetIsAttacking() const { return CombatComponent ? CombatComponent->GetIsAttacking() : false; }

void AMyProjectCharacter::SetIsInRollAnimation(bool value) { /* maintained for BP compatibility */ }

float AMyProjectCharacter::GetLookRotation()
{
	return RotationSmoothingComponent ? RotationSmoothingComponent->GetCurrentOffset() : 0.f;
}

void AMyProjectCharacter::SetIsAttackEnding(bool value) { if (CombatComponent) CombatComponent->SetIsAttackEnding(value); }
void AMyProjectCharacter::SetIsSecondAttackWindowOpen(bool value)
{
	if (HasAuthority())
	{
		if (CombatComponent) CombatComponent->SetIsSecondAttackWindowOpen(value);
	}
	else
	{
		ServerSetSecondAttackWindow(value);
	}
}

void AMyProjectCharacter::FireProjectile()
{
	if (ProjectileSpawnerComponent && ProjectileClass)
	{
		ProjectileSpawnerComponent->SpawnProjectile(ProjectileClass, this);
	}
}

// Input
void AMyProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	_PlayerInputComponent = PlayerInputComponent;
	InputHandler->SetupPlayerInputComponent(_PlayerInputComponent, GetController());
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
		PlayerInputComponent->BindTouch(IE_Pressed, this, &AMyProjectCharacter::OnSwipeStarted);
		PlayerInputComponent->BindTouch(IE_Repeat, this, &AMyProjectCharacter::OnSwipeUpdated);
		PlayerInputComponent->BindTouch(IE_Released, this, &AMyProjectCharacter::OnSwipeEnded);
	}
}

void AMyProjectCharacter::Tick(float DeltaTime)
{
	// No smoothing logic here; handled inside RotationSmoothingComponent.
}

void AMyProjectCharacter::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PC = Cast<APlayerController>(GetController());
}

void AMyProjectCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate variables
	DOREPLIFETIME(AMyProjectCharacter, IsPlayerTryingToMove);
}

void AMyProjectCharacter::OnSwipeStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	GestureRecognizer->StartGesture(Location);
}

void AMyProjectCharacter::OnSwipeUpdated(ETouchIndex::Type FingerIndex, FVector Location)
{
	GestureRecognizer->UpdateGesture(Location);
}

void AMyProjectCharacter::HandleGesture(EGestureType Gesture)
{
	if (Gesture == EGestureType::SwipeDown)
	{
		if (HasAuthority()) MulticastStartDodge(); else ServerStartDodge();
	}
	else if (Gesture == EGestureType::None)
	{
		std::random_device rd; std::mt19937 gen(rd()); std::uniform_real_distribution<float> dis(-90.f, 90.f);
		const float Angle = dis(gen);
		if (HasAuthority()) MulticastStartAttack(Angle); else ServerStartAttack(Angle);
	}
}

void AMyProjectCharacter::OnSwipeEnded(ETouchIndex::Type FingerIndex, FVector Location)
{
	GestureRecognizer->EndGesture(Location);
}

// Attack & dodge triggered directly through CombatComponent (wrappers removed).

void AMyProjectCharacter::SwitchToWalking()
{
	GetCharacterMovement()->MaxWalkSpeed = 200.f;
}

void AMyProjectCharacter::SwitchToRunning()
{
	SmoothlyRotate(0, 10);
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}

void AMyProjectCharacter::SetMovementVector(FVector2D _MovementVector)
{
	MovementVector = _MovementVector;
}

void AMyProjectCharacter::ServerSetIsPlayerTryingToMove_Implementation(bool NewIsPlayerTryingToMove)
{
	IsPlayerTryingToMove = NewIsPlayerTryingToMove;
}

bool AMyProjectCharacter::ServerSetIsPlayerTryingToMove_Validate(bool NewIsPlayerTryingToMove)
{
	return true;
}

void AMyProjectCharacter::ServerStartDodge_Implementation() { MulticastStartDodge(); }

bool AMyProjectCharacter::ServerStartDodge_Validate()
{
	return true;
}

void AMyProjectCharacter::MulticastStartDodge_Implementation() { if (CombatComponent) CombatComponent->StartDodge(); }

void AMyProjectCharacter::ServerStartAttack_Implementation(float angle)
{
	MulticastStartAttack(angle);
}

bool AMyProjectCharacter::ServerStartAttack_Validate(float angle)
{
	return true;
}

void AMyProjectCharacter::MulticastStartAttack_Implementation(float angle) { SmoothlyRotate(angle, 1); if (CombatComponent) CombatComponent->StartAttack(); }

// StartDodge wrapper removed.
// Callback when rotation offset changes
void AMyProjectCharacter::HandleRotationOffsetChanged(float NewOffset)
{
	if (ProjectileSpawnerComponent)
	{
		ProjectileSpawnerComponent->UpdateFromRotationOffset(NewOffset);
	}
	// Keep legacy arrow (if exists) in sync
	if (ProjectileSpawnPoint)
	{
		const float Rad = FMath::DegreesToRadians(NewOffset + 90.f);
		ProjectileSpawnPoint->SetRelativeRotation(FRotator(0.f, -NewOffset, 0.f));
		ProjectileSpawnPoint->SetRelativeLocation(FVector(100.f * FMath::Sin(Rad), 100.f * FMath::Cos(Rad), 50.f));
	}
}

void AMyProjectCharacter::ServerSetSecondAttackWindow_Implementation(bool bOpen)
{
	if (CombatComponent) CombatComponent->SetIsSecondAttackWindowOpen(bOpen);
}

bool AMyProjectCharacter::ServerSetSecondAttackWindow_Validate(bool bOpen)
{
	return true;
}

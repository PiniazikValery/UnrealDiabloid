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
#include "Character/CharacterStatsComponent.h"

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

	StatsComponent = CreateDefaultSubobject<UCharacterStatsComponent>(TEXT("StatsComponent"));
	if (StatsComponent)
	{
		StatsComponent->OnDied.AddDynamic(this, &AMyProjectCharacter::HandleDeath);
	}

	// Load default stats widget class (can be overridden in BP)
	static ConstructorHelpers::FClassFinder<UUserWidget> StatsWidgetBP(TEXT("/Game/UI/CharacterStats.CharacterStats_C"));
	if (StatsWidgetBP.Succeeded())
	{
		CharacterStatsWidgetClass = StatsWidgetBP.Class;
	}
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

	// For now, use the same action as Roll for Dodge (can be changed to a separate action later)
	static ConstructorHelpers::FObjectFinder<UInputAction> DodgeInputActionAsset(TEXT("/Game/ThirdPerson/Input/Actions/IA_Roll.IA_Roll"));
	if (DodgeInputActionAsset.Succeeded()) DodgeAction = DodgeInputActionAsset.Object;
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

bool AMyProjectCharacter::GetIsDodging() const 
{ 
	UMyCharacterMovementComponent* MyMovement = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
	return MyMovement ? MyMovement->bIsDodging : false;
}
bool AMyProjectCharacter::GetIsAttacking() const { return CombatComponent ? CombatComponent->GetIsAttacking() : false; }

void AMyProjectCharacter::SetIsInRollAnimation(bool value) { /* maintained for BP compatibility */ }

float AMyProjectCharacter::GetLookRotation()
{
	return RotationSmoothingComponent ? RotationSmoothingComponent->GetCurrentOffset() : 0.f;
}

void AMyProjectCharacter::SetIsAttackEnding(bool value) { if (CombatComponent) CombatComponent->SetIsAttackEnding(value); }
void AMyProjectCharacter::SetIsSecondAttackWindowOpen(bool value)
{
	// Only server mutates the CombatComponent state. Only owning client is allowed to ask.
	if (HasAuthority())
	{
		if (CombatComponent) CombatComponent->SetIsSecondAttackWindowOpen(value);
		return;
	}
	// Reject calls from simulated proxies (no owning connection)
	if (!IsLocallyControlled())
	{
		return;
	}
	ServerSetSecondAttackWindow(value);
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
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] SetupPlayerInputComponent called: HasAuthority=%s, IsLocallyControlled=%s"),
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));

	_PlayerInputComponent = PlayerInputComponent;
	
	// Make sure we have a valid input component
	if (!PlayerInputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] SetupPlayerInputComponent: PlayerInputComponent is NULL!"),
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
		return;
	}

	// Set up your other input handlers
	if (InputHandler)
	{
		InputHandler->SetupPlayerInputComponent(_PlayerInputComponent, GetController());
	}

	// Add mapping context - only for locally controlled
	if (IsLocallyControlled())
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	// Set up enhanced input bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Setting up Enhanced Input bindings"),
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));

		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		if (RollAction)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] Binding RollAction"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
			EnhancedInputComponent->BindAction(RollAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::OnRoll);
		}

		if (DodgeAction)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] Binding DodgeAction"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::OnDodge);
		}
	}

	// Touch input bindings - make sure these are set up with proper priority
	UE_LOG(LogTemp, Warning, TEXT("[%s] Binding touch events"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
	
	// Use direct binding for more control
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AMyProjectCharacter::OnSwipeStarted);
	PlayerInputComponent->BindTouch(IE_Repeat, this, &AMyProjectCharacter::OnSwipeUpdated);
	PlayerInputComponent->BindTouch(IE_Released, this, &AMyProjectCharacter::OnSwipeEnded);
	
	// Set input priority to ensure touch events aren't consumed by other components
	PlayerInputComponent->Priority = 1;
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] Touch bindings complete. Input component priority: %d"),
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		PlayerInputComponent->Priority);
}

void AMyProjectCharacter::Tick(float DeltaTime)
{
	// No smoothing logic here; handled inside RotationSmoothingComponent.
}

void AMyProjectCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] BeginPlay called: HasAuthority=%s, IsLocallyControlled=%s, Controller=%s"), 
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		GetController() ? *GetController()->GetName() : TEXT("NULL"));
	
	// Debug GestureRecognizer status for locally controlled characters
	if (IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] GestureRecognizer: %s"),
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
			GestureRecognizer ? TEXT("Valid") : TEXT("NULL"));
	}
	
	// For clients, ensure input is set up
	if (!HasAuthority() && IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->InputComponent && !_PlayerInputComponent)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CLIENT] BeginPlay: Setting up client input"));
				SetupPlayerInputComponent(PC->InputComponent);
			}
		}
	}
	
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && IsLocallyControlled() && CharacterStatsWidgetClass && !CharacterStatsWidget)
	{
		CharacterStatsWidget = CreateWidget<UUserWidget>(PC, CharacterStatsWidgetClass);
		if (CharacterStatsWidget)
		{
			CharacterStatsWidget->AddToViewport();
		}
	}
}

void AMyProjectCharacter::PossessedBy(AController* NewController)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] PossessedBy called: NewController=%s, HasAuthority=%s"), 
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		NewController ? *NewController->GetName() : TEXT("NULL"),
		HasAuthority() ? TEXT("true") : TEXT("false"));
	
	Super::PossessedBy(NewController);
	
	// Force input setup if this is a PlayerController and we haven't set it up yet
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] PossessedBy: PlayerController assigned, ensuring input setup"), 
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
		
		UE_LOG(LogTemp, Warning, TEXT("[%s] PossessedBy: Debug - PC->InputComponent=%s, _PlayerInputComponent=%s"), 
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
			PC->InputComponent ? TEXT("Valid") : TEXT("NULL"),
			_PlayerInputComponent ? TEXT("Valid") : TEXT("NULL"));
		
		// Manually call SetupPlayerInputComponent since it wasn't called in BeginPlay (controller was NULL)
		if (PC->InputComponent && !_PlayerInputComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] PossessedBy: Manually calling SetupPlayerInputComponent"), 
				HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
			SetupPlayerInputComponent(PC->InputComponent);
		}
		else if (!PC->InputComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] PossessedBy: InputComponent not ready, will retry next tick"), 
				HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
			// InputComponent not ready yet, schedule a retry on next tick
			InputSetupRetryCount = 0; // Reset retry count
			GetWorld()->GetTimerManager().SetTimer(InputSetupRetryTimer, this, &AMyProjectCharacter::RetryInputSetup, 0.1f, false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] PossessedBy: NOT calling SetupPlayerInputComponent - PC->InputComponent=%s, _PlayerInputComponent=%s"), 
				HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
				PC->InputComponent ? TEXT("Valid") : TEXT("NULL"),
				_PlayerInputComponent ? TEXT("Valid") : TEXT("NULL"));
		}
	}
}

void AMyProjectCharacter::RetryInputSetup()
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] RetryInputSetup called"), 
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RetryInputSetup: PC->InputComponent=%s, _PlayerInputComponent=%s"), 
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
			PC->InputComponent ? TEXT("Valid") : TEXT("NULL"),
			_PlayerInputComponent ? TEXT("Valid") : TEXT("NULL"));
		
		if (PC->InputComponent && !_PlayerInputComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] RetryInputSetup: InputComponent now ready, calling SetupPlayerInputComponent"), 
				HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
			SetupPlayerInputComponent(PC->InputComponent);
		}
		else if (!PC->InputComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] RetryInputSetup: InputComponent still not ready, scheduling another retry"), 
				HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
			// Still not ready, try again in another 0.1 seconds (max 5 retries)
			if (++InputSetupRetryCount < 5)
			{
				GetWorld()->GetTimerManager().SetTimer(InputSetupRetryTimer, this, &AMyProjectCharacter::RetryInputSetup, 0.1f, false);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[%s] RetryInputSetup: Failed to setup input after 5 retries"), 
					HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
				InputSetupRetryCount = 0; // Reset for next time
			}
		}
	}
}

void AMyProjectCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] OnRep_PlayerState called: Controller=%s"), 
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		GetController() ? *GetController()->GetName() : TEXT("NULL"));
	
	// This is called on clients when PlayerState replicates
	// Ensure input setup happens on client side
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->InputComponent && !_PlayerInputComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] OnRep_PlayerState: Setting up client input"), 
				HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
			SetupPlayerInputComponent(PC->InputComponent);
		}
	}
}

void AMyProjectCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	
	UE_LOG(LogTemp, Warning, TEXT("[CLIENT] OnRep_Controller called: Controller=%s, IsLocallyControlled=%s"),
		GetController() ? *GetController()->GetName() : TEXT("NULL"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	
	// Set up input on the client when the controller replicates
	if (IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->InputComponent && !_PlayerInputComponent)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CLIENT] OnRep_Controller: Setting up input on client"));
				SetupPlayerInputComponent(PC->InputComponent);
			}
			else if (!PC->InputComponent)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CLIENT] OnRep_Controller: InputComponent not ready, scheduling retry"));
				GetWorld()->GetTimerManager().SetTimer(InputSetupRetryTimer, this, &AMyProjectCharacter::RetryInputSetup, 0.1f, false);
			}
		}
	}
}

void AMyProjectCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate variables
	DOREPLIFETIME(AMyProjectCharacter, IsPlayerTryingToMove);
	// (Stats handled inside UCharacterStatsComponent)
}

void AMyProjectCharacter::OnSwipeStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] OnSwipeStarted: FingerIndex=%d, Location=(%f,%f,%f), IsLocallyControlled=%s"),
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), 
		(int32)FingerIndex, 
		Location.X, Location.Y, Location.Z,
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	
	if (IsLocallyControlled() && GestureRecognizer)
	{
		GestureRecognizer->StartGesture(Location);
	}
}

void AMyProjectCharacter::OnSwipeUpdated(ETouchIndex::Type FingerIndex, FVector Location)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] OnSwipeUpdated called"), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
	// Only process on locally controlled characters
	if (IsLocallyControlled() && GestureRecognizer)
	{
		GestureRecognizer->UpdateGesture(Location);
	}
}

void AMyProjectCharacter::HandleGesture(EGestureType Gesture)
{
	if (Gesture == EGestureType::SwipeDown)
	{
		// Use the new movement component dodge system for swipe down
		if (IsLocallyControlled())
		{
			UMyCharacterMovementComponent* MyMovement = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
			if (MyMovement)
			{
				MyMovement->StartDodge();
			}
		}
	}
	else if (Gesture == EGestureType::None)
	{
		// Use the new movement component dodge system for swipe down
		if (IsLocallyControlled())
		{
			UMyCharacterMovementComponent* MyMovement = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
			if (MyMovement)
			{
				MyMovement->StartDodge();
			}
		}
		// std::random_device rd; std::mt19937 gen(rd()); std::uniform_real_distribution<float> dis(-90.f, 90.f);
		// const float Angle = dis(gen);
		// if (HasAuthority()) MulticastStartAttack(Angle); else ServerStartAttack(Angle);
	}
}

void AMyProjectCharacter::OnSwipeEnded(ETouchIndex::Type FingerIndex, FVector Location)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] OnSwipeEnded: FingerIndex=%d, Location=(%f,%f,%f), IsLocallyControlled=%s"),
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		(int32)FingerIndex,
		Location.X, Location.Y, Location.Z,
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	
	if (IsLocallyControlled() && GestureRecognizer)
	{
		GestureRecognizer->EndGesture(Location);
	}
}

void AMyProjectCharacter::OnRoll()
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] OnRoll (Enhanced Input) called: HasAuthority=%s, IsLocallyControlled=%s"), 
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	
	// Route through gesture system for consistency, treating it as a swipe down gesture
	if (IsLocallyControlled() && CombatComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] OnRoll: Calling CombatComponent->StartDodge()"), 
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
		CombatComponent->StartDodge(); // handles prediction + server call internally
	}
}

void AMyProjectCharacter::OnDodge()
{
	// Performance logging for input spam detection
	static int32 InputCallCount = 0;
	static float LastInputLogTime = 0.0f;
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	
	InputCallCount++;
	
	// Log every 5 input calls or every 1 second
	if (InputCallCount % 5 == 0 || (CurrentTime - LastInputLogTime) > 1.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("PERFORMANCE: OnDodge input called %d times in %.2f seconds"), 
		       InputCallCount, CurrentTime - LastInputLogTime);
		InputCallCount = 0;
		LastInputLogTime = CurrentTime;
	}
	
	// Use the new movement component dodge system
	if (IsLocallyControlled())
	{
		UMyCharacterMovementComponent* MyMovement = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
		if (MyMovement)
		{
			const double StartTime = FPlatformTime::Seconds();
			MyMovement->StartDodge();
			const double EndTime = FPlatformTime::Seconds();
			const double ExecutionTime = (EndTime - StartTime) * 1000.0;
			
			// Log if dodge call takes too long
			if (ExecutionTime > 0.1)
			{
				UE_LOG(LogTemp, Warning, TEXT("PERFORMANCE: StartDodge call took %.3f ms"), ExecutionTime);
			}
		}
	}
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

// Dodge RPC wrappers removed (handled in component)

void AMyProjectCharacter::ServerStartAttack_Implementation(float angle)
{
	MulticastStartAttack(angle);
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

// ========= Stats Integration =========
float AMyProjectCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	return StatsComponent ? StatsComponent->ApplyDamage(DamageAmount) : 0.f;
}

void AMyProjectCharacter::HandleDeath()
{
	GetCharacterMovement()->DisableMovement();
}

bool AMyProjectCharacter::SpendMana(float Amount)
{
	return StatsComponent ? StatsComponent->SpendMana(Amount) : false;
}

void AMyProjectCharacter::RestoreMana(float Amount)
{
	if (StatsComponent) StatsComponent->RestoreMana(Amount);
}

void AMyProjectCharacter::Heal(float Amount)
{
	if (StatsComponent) StatsComponent->Heal(Amount);
}

bool AMyProjectCharacter::IsAlive() const
{
	return StatsComponent ? StatsComponent->IsAlive() : false;
}

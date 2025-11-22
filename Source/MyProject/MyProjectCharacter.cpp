// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectCharacter.h"
#include "CharacterConfigurationAsset.h"
#include "Components/CharacterSetupComponent.h"
#include "Components/CharacterNetworkComponent.h"
#include "Character/CharacterAnimationComponent.h"
#include "EnemyCharacter.h"
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
#include <random>
#include <cmath>
#include "AIController.h"
#include "Components/RotationSmoothingComponent.h"
#include "Components/ProjectileSpawnerComponent.h"
#include "Character/CharacterStatsComponent.h"
#include "Moves/Dodge.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"

AMyProjectCharacter::AMyProjectCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Enable replication
	bReplicates = true;
	SetReplicateMovement(true);
	
	// ===== CAPSULE INITIALIZATION =====
	// Initialize capsule size FIRST to ensure consistent size across network
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	
	// ===== COMPONENT CREATION =====
	// Components are created here because they must exist before PostInitializeComponents()
	// Configuration of components is handled in PostInitializeComponents() by SetupComponent
	
	// Create the setup component FIRST
	SetupComponent = CreateDefaultSubobject<UCharacterSetupComponent>(TEXT("SetupComponent"));
	
	// Create the network component to handle all RPCs and replication
	NetworkComponent = CreateDefaultSubobject<UCharacterNetworkComponent>(TEXT("NetworkComponent"));
	
	// Load default configuration asset
	static ConstructorHelpers::FObjectFinder<UCharacterConfigurationAsset> DefaultConfig(TEXT("/Game/Config/DA_DefaultCharacterConfig.DA_DefaultCharacterConfig"));
	if (DefaultConfig.Succeeded())
	{
		CharacterConfig = DefaultConfig.Object;
	}
	
	// Initialize mesh in constructor to ensure it's set before replication
	InitializeMesh();
	
	// Create visual components (creation stays in constructor, configuration in SetupComponent)
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), TEXT("weapon_r"));
	
	// Create camera hierarchy
	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	CameraRoot->SetupAttachment(RootComponent);
	CameraRoot->SetUsingAbsoluteRotation(true);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CameraRoot);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	
	ProjectileSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(RootComponent);

	// Replication already enabled at start of constructor
	InputHandler = CreateDefaultSubobject<UCharacterInput>(TEXT("InputHandler"));
	GestureRecognizer = CreateDefaultSubobject<UMyGestureRecognizer>(TEXT("GestureRecognizer"));
	AnimationComponent = CreateDefaultSubobject<UCharacterAnimationComponent>(TEXT("AnimationComponent"));
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	RotationSmoothingComponent = CreateDefaultSubobject<URotationSmoothingComponent>(TEXT("RotationSmoothingComponent"));
	ProjectileSpawnerComponent = CreateDefaultSubobject<UProjectileSpawnerComponent>(TEXT("ProjectileSpawnerComponent"));
	StatsComponent = CreateDefaultSubobject<UCharacterStatsComponent>(TEXT("StatsComponent"));
	
	// ===== EVENT BINDING =====
	// Event bindings must happen in constructor before BeginPlay
	
	if (RotationSmoothingComponent && ProjectileSpawnerComponent)
	{
		RotationSmoothingComponent->OnRotationOffsetChanged.AddDynamic(this, &AMyProjectCharacter::HandleRotationOffsetChanged);
	}
	GestureRecognizer->OnGestureRecognized.AddDynamic(this, &AMyProjectCharacter::HandleGesture);
	
	if (StatsComponent)
	{
		StatsComponent->OnDied.AddDynamic(this, &AMyProjectCharacter::HandleDeath);
	}
	
	// Kept for input initialization (input setup is separate from visual setup)
	InitializeInput();
	
	// Load projectile class and UI from config
	if (CharacterConfig)
	{
		ProjectileClass = CharacterConfig->ProjectileClass;
		CharacterStatsWidgetClass = CharacterConfig->StatsWidgetClass;
		
		// Set melee combat parameters on CombatComponent
		if (CombatComponent)
		{
			CombatComponent->MeleeDamage = CharacterConfig->MeleeDamage;
			CombatComponent->MeleeRange = CharacterConfig->MeleeRange;
		}
	}
	else
	{
		// Fallback to defaults if no config
		ProjectileClass = AMageProjectile::StaticClass();
		
		static ConstructorHelpers::FClassFinder<UUserWidget> StatsWidgetBP(TEXT("/Game/UI/CharacterStats.CharacterStats_C"));
		if (StatsWidgetBP.Succeeded())
		{
			CharacterStatsWidgetClass = StatsWidgetBP.Class;
		}
	}
}

void AMyProjectCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	UE_LOG(LogTemp, Log, TEXT("PostInitializeComponents: Calling SetupComponent to initialize character..."));
	
	// NEW CODE: Use setup component to configure all visual and movement components
	if (SetupComponent)
	{
		SetupComponent->InitializeCharacter(this);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PostInitializeComponents: SetupComponent is NULL!"));
	}
	
	// Capsule collision setup (moved from constructor)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	
	// AnimationComponent initialization now handled by SetupComponent
}

void AMyProjectCharacter::InitializeMesh()
{
	// Setup mesh transform FIRST (before setting skeletal mesh)
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Error, TEXT("InitializeMesh: No mesh component!"));
		return;
	}
	
	// Set transform
	MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	MeshComp->SetRelativeRotation(FRotator(0.0f, 270.0f, 0.0f));
	
	// Set collision
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	// Load and set mesh from config (if available)
	if (CharacterConfig)
	{
		// Load skeletal mesh
		if (!CharacterConfig->CharacterMesh.IsNull())
		{
			USkeletalMesh* LoadedMesh = CharacterConfig->CharacterMesh.LoadSynchronous();
			if (LoadedMesh)
			{
				MeshComp->SetSkeletalMesh(LoadedMesh);
				UE_LOG(LogTemp, Log, TEXT("InitializeMesh: Skeletal mesh loaded from config"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("InitializeMesh: Failed to load skeletal mesh"));
			}
		}
		
		// Set animation blueprint
		if (CharacterConfig->AnimationBlueprint)
		{
			MeshComp->SetAnimInstanceClass(CharacterConfig->AnimationBlueprint);
			UE_LOG(LogTemp, Log, TEXT("InitializeMesh: Animation blueprint set from config"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("InitializeMesh: CharacterConfig is NULL - mesh not loaded"));
	}
}

void AMyProjectCharacter::InitializeInput()
{
	if (!CharacterConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterConfig is not set! Cannot initialize input."));
		return;
	}
	
	// Load input assets from config
	if (!CharacterConfig->DefaultMappingContext.IsNull())
	{
		DefaultMappingContext = CharacterConfig->DefaultMappingContext.LoadSynchronous();
	}
	
	if (!CharacterConfig->JumpAction.IsNull())
	{
		JumpAction = CharacterConfig->JumpAction.LoadSynchronous();
	}
	
	if (!CharacterConfig->RollAction.IsNull())
	{
		RollAction = CharacterConfig->RollAction.LoadSynchronous();
	}
	
	if (!CharacterConfig->DodgeAction.IsNull())
	{
		DodgeAction = CharacterConfig->DodgeAction.LoadSynchronous();
	}
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

// ========= Network Component Wrappers (Backward Compatibility) =========

bool AMyProjectCharacter::GetIsPlayerTryingToMove() const
{
	return NetworkComponent ? NetworkComponent->GetIsPlayerTryingToMove() : false;
}

void AMyProjectCharacter::SetIsPlayerTryingToMove(bool bValue)
{
	if (NetworkComponent)
	{
		NetworkComponent->SetIsPlayerTryingToMove(bValue);
	}
}

// ========= Movement Functions =========

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
	if (NetworkComponent)
	{
		NetworkComponent->SetSecondAttackWindow(value);
	}
}

void AMyProjectCharacter::SetIsAttacking(bool value)
{
	if (CombatComponent)
	{
		CombatComponent->bIsAttacking = value;
	}
}

void AMyProjectCharacter::DetectHit()
{
	// Delegate to CombatComponent for hit detection
	if (CombatComponent)
	{
		CombatComponent->DetectHit();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DetectHit: CombatComponent is null!"));
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
			// Bind the StatsComponent to the widget's CharacterStats property
			if (StatsComponent)
			{
				FProperty* Property = CharacterStatsWidget->GetClass()->FindPropertyByName(FName("CharacterStats"));
				if (Property)
				{
					FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);
					if (ObjectProperty)
					{
						void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(CharacterStatsWidget);
						ObjectProperty->SetObjectPropertyValue(PropertyAddress, StatsComponent);
					}
				}
			}
			CharacterStatsWidget->AddToViewport();
		}
	}
	
	// Bind to animation events
	if (AnimationComponent)
	{
		AnimationComponent->OnAnimationComplete.AddDynamic(this, &AMyProjectCharacter::HandleAnimationComplete);
		AnimationComponent->OnAnimationStarted.AddDynamic(this, &AMyProjectCharacter::HandleAnimationStarted);
		UE_LOG(LogTemp, Log, TEXT("BeginPlay: Bound to AnimationComponent events"));
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

	// Network replication now handled by UCharacterNetworkComponent
	// Stats handled inside UCharacterStatsComponent
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
		std::random_device rd; std::mt19937 gen(rd()); std::uniform_real_distribution<float> dis(-90.f, 90.f);
		const float Angle = dis(gen);
		if (NetworkComponent)
		{
			NetworkComponent->TriggerAttack(Angle);
		}
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
	if (CharacterConfig)
	{
		GetCharacterMovement()->MaxWalkSpeed = CharacterConfig->WalkSpeed;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = 200.f; // Fallback
	}
}

void AMyProjectCharacter::SwitchToRunning()
{
	SmoothlyRotate(0, 10);
	
	if (CharacterConfig)
	{
		GetCharacterMovement()->MaxWalkSpeed = CharacterConfig->RunSpeed;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = 500.f; // Fallback
	}
}

void AMyProjectCharacter::SetMovementVector(FVector2D _MovementVector)
{
	MovementVector = _MovementVector;
}

// Attack & Dodge RPC wrappers removed (handled in NetworkComponent)
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

// ========= Animation Event Handlers =========
void AMyProjectCharacter::HandleAnimationComplete(FName AnimationName)
{
	UE_LOG(LogTemp, Log, TEXT("Animation completed: %s"), *AnimationName.ToString());
	
	// React to specific animations
	if (AnimationName == TEXT("FirstAttack"))
	{
		// First attack finished, ready for next action
		if (CombatComponent)
		{
			// Notify combat component that attack animation finished
			UE_LOG(LogTemp, Log, TEXT("First attack animation finished"));
		}
	}
	else if (AnimationName == TEXT("SecondAttack"))
	{
		// Combo finished
		if (CombatComponent)
		{
			// Notify combat component that combo finished
			UE_LOG(LogTemp, Log, TEXT("Second attack animation finished"));
		}
	}
}

void AMyProjectCharacter::HandleAnimationStarted(FName AnimationName)
{
	UE_LOG(LogTemp, Log, TEXT("Animation started: %s"), *AnimationName.ToString());
}

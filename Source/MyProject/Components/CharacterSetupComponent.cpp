// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterSetupComponent.h"
#include "../MyProjectCharacter.h"
#include "../CharacterConfigurationAsset.h"
#include "../Character/CharacterAnimationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"

UCharacterSetupComponent::UCharacterSetupComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void UCharacterSetupComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCharacterSetupComponent::InitializeCharacter(AMyProjectCharacter* Character)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("CharacterSetupComponent: Already initialized!"));
		return;
	}

	if (!ValidateSetup(Character))
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterSetupComponent: Validation failed!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("CharacterSetupComponent: Initializing character..."));

	OwnerCharacter = Character;

	// Call setup methods in order
	SetupCollision(Character);
	SetupMesh(Character);
	SetupWeapon(Character);
	SetupAnimation(Character);
	SetupAnimationComponent(Character);
	SetupMovement(Character);
	SetupCamera(Character);
	SetupProjectileSpawnPoint(Character);

	bIsInitialized = true;
	UE_LOG(LogTemp, Log, TEXT("CharacterSetupComponent: Initialization complete!"));
}

bool UCharacterSetupComponent::ValidateSetup(const AMyProjectCharacter* Character) const
{
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterSetupComponent: Character is null!"));
		return false;
	}

	if (!Character->GetMesh())
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterSetupComponent: Character mesh component is missing!"));
		return false;
	}

	if (!Character->GetCharacterMovement())
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterSetupComponent: Movement component is missing!"));
		return false;
	}

	if (!Character->GetCapsuleComponent())
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterSetupComponent: Capsule component is missing!"));
		return false;
	}

	return true;
}

void UCharacterSetupComponent::SetupCollision(AMyProjectCharacter* Character)
{
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupCollision: No capsule component!"));
		return;
	}

	// DO NOT call InitCapsuleSize() here!
	// Capsule size is initialized in the character constructor
	
	// Only set collision responses
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	
	UE_LOG(LogTemp, Log, TEXT("SetupCollision: Collision responses configured"));
}

void UCharacterSetupComponent::SetupMesh(AMyProjectCharacter* Character)
{
	// Mesh is now set in the character constructor to ensure it's
	// initialized before network replication occurs.
	// This method now only validates that the mesh is properly configured.
	
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupMesh: No mesh component!"));
		return;
	}

	// Validate that mesh was set in constructor
	if (!Mesh->GetSkeletalMeshAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupMesh: No skeletal mesh set! Check CharacterConfig in constructor."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("SetupMesh: Mesh validated successfully (Set in constructor)"));
	}
	
	// Just ensure collision settings are correct (defensive)
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	// Log mesh state for debugging
	FVector MeshLocation = Mesh->GetRelativeLocation();
	FRotator MeshRotation = Mesh->GetRelativeRotation();
	UE_LOG(LogTemp, Log, TEXT("SetupMesh: Mesh transform - Location: %s, Rotation: %s"),
		*MeshLocation.ToString(), *MeshRotation.ToString());
}

void UCharacterSetupComponent::SetupWeapon(AMyProjectCharacter* Character)
{
	// Get weapon mesh through reflection or direct access
	UStaticMeshComponent* WeaponMesh = nullptr;
	FProperty* WeaponProp = Character->GetClass()->FindPropertyByName(FName("WeaponMesh"));
	if (WeaponProp)
	{
		FObjectProperty* ObjectProp = CastField<FObjectProperty>(WeaponProp);
		if (ObjectProp)
		{
			void* PropAddress = WeaponProp->ContainerPtrToValuePtr<void>(Character);
			WeaponMesh = Cast<UStaticMeshComponent>(ObjectProp->GetObjectPropertyValue(PropAddress));
		}
	}

	if (!WeaponMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupWeapon: No weapon mesh component!"));
		return;
	}

	// Get config from character
	UCharacterConfigurationAsset* Config = nullptr;
	FProperty* ConfigProp = Character->GetClass()->FindPropertyByName(FName("CharacterConfig"));
	if (ConfigProp)
	{
		FObjectProperty* ObjectProp = CastField<FObjectProperty>(ConfigProp);
		if (ObjectProp)
		{
			void* PropAddress = ConfigProp->ContainerPtrToValuePtr<void>(Character);
			Config = Cast<UCharacterConfigurationAsset>(ObjectProp->GetObjectPropertyValue(PropAddress));
		}
	}

	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupWeapon: CharacterConfig is not set!"));
		return;
	}

	// Load weapon mesh from config
	if (!Config->WeaponMesh.IsNull())
	{
		UStaticMesh* LoadedWeapon = Config->WeaponMesh.LoadSynchronous();
		if (LoadedWeapon)
		{
			WeaponMesh->SetStaticMesh(LoadedWeapon);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			
			UE_LOG(LogTemp, Log, TEXT("SetupWeapon: Weapon configured"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SetupWeapon: Failed to load weapon mesh"));
		}
	}
}

void UCharacterSetupComponent::SetupAnimation(AMyProjectCharacter* Character)
{
	// Animation blueprint is now set in the character constructor
	// This method only validates that it's properly configured
	
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupAnimation: No mesh component!"));
		return;
	}

	// Validate animation blueprint was set
	if (Mesh->GetAnimInstance())
	{
		UE_LOG(LogTemp, Log, TEXT("SetupAnimation: Animation instance validated (Set in constructor)"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupAnimation: No animation instance! Check AnimationBlueprint in CharacterConfig."));
	}
}

void UCharacterSetupComponent::SetupAnimationComponent(AMyProjectCharacter* Character)
{
	// Get AnimationComponent through reflection
	UObject* AnimCompObj = nullptr;
	FProperty* AnimCompProp = Character->GetClass()->FindPropertyByName(FName("AnimationComponent"));
	if (AnimCompProp)
	{
		FObjectProperty* ObjectProp = CastField<FObjectProperty>(AnimCompProp);
		if (ObjectProp)
		{
			void* PropAddress = AnimCompProp->ContainerPtrToValuePtr<void>(Character);
			AnimCompObj = ObjectProp->GetObjectPropertyValue(PropAddress);
		}
	}

	class UCharacterAnimationComponent* AnimationComponent = Cast<UCharacterAnimationComponent>(AnimCompObj);
	if (!AnimationComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupAnimationComponent: No AnimationComponent found!"));
		return;
	}

	// Get config from character
	UCharacterConfigurationAsset* Config = nullptr;
	FProperty* ConfigProp = Character->GetClass()->FindPropertyByName(FName("CharacterConfig"));
	if (ConfigProp)
	{
		FObjectProperty* ObjectProp = CastField<FObjectProperty>(ConfigProp);
		if (ObjectProp)
		{
			void* PropAddress = ConfigProp->ContainerPtrToValuePtr<void>(Character);
			Config = Cast<UCharacterConfigurationAsset>(ObjectProp->GetObjectPropertyValue(PropAddress));
		}
	}

	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupAnimationComponent: CharacterConfig is not set!"));
		return;
	}

	// Load animation montages from config
	UAnimMontage* StartF = Config->GetAnimationMontage(UCharacterConfigurationAsset::MONTAGE_START_F);
	UAnimMontage* StartR = Config->GetAnimationMontage(UCharacterConfigurationAsset::MONTAGE_START_R);
	UAnimMontage* Attack1 = Config->GetAnimationMontage(UCharacterConfigurationAsset::MONTAGE_ATTACK_1);
	UAnimMontage* Attack2 = Config->GetAnimationMontage(UCharacterConfigurationAsset::MONTAGE_ATTACK_2);
	
	// Set montages in the component
	AnimationComponent->SetAnimationMontages(StartF, StartR, Attack1, Attack2);
	
	// Initialize the component
	AnimationComponent->Initialize();
	
	// BACKWARD COMPATIBILITY: Set references on character for old code
	FProperty* StartFProp = Character->GetClass()->FindPropertyByName(FName("StartFMontage"));
	if (StartFProp)
	{
		FObjectProperty* ObjProp = CastField<FObjectProperty>(StartFProp);
		if (ObjProp)
		{
			void* PropAddr = StartFProp->ContainerPtrToValuePtr<void>(Character);
			ObjProp->SetObjectPropertyValue(PropAddr, StartF);
		}
	}
	
	FProperty* StartRProp = Character->GetClass()->FindPropertyByName(FName("StartRMontage"));
	if (StartRProp)
	{
		FObjectProperty* ObjProp = CastField<FObjectProperty>(StartRProp);
		if (ObjProp)
		{
			void* PropAddr = StartRProp->ContainerPtrToValuePtr<void>(Character);
			ObjProp->SetObjectPropertyValue(PropAddr, StartR);
		}
	}
	
	FProperty* Attack1Prop = Character->GetClass()->FindPropertyByName(FName("FirstAttackMontage"));
	if (Attack1Prop)
	{
		FObjectProperty* ObjProp = CastField<FObjectProperty>(Attack1Prop);
		if (ObjProp)
		{
			void* PropAddr = Attack1Prop->ContainerPtrToValuePtr<void>(Character);
			ObjProp->SetObjectPropertyValue(PropAddr, Attack1);
		}
	}
	
	FProperty* Attack2Prop = Character->GetClass()->FindPropertyByName(FName("SecondAttackMontage"));
	if (Attack2Prop)
	{
		FObjectProperty* ObjProp = CastField<FObjectProperty>(Attack2Prop);
		if (ObjProp)
		{
			void* PropAddr = Attack2Prop->ContainerPtrToValuePtr<void>(Character);
			ObjProp->SetObjectPropertyValue(PropAddr, Attack2);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("SetupAnimationComponent: AnimationComponent initialized with montages"));
}

void UCharacterSetupComponent::SetupMovement(AMyProjectCharacter* Character)
{
	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupMovement: No movement component!"));
		return;
	}

	// Get config from character
	UCharacterConfigurationAsset* Config = nullptr;
	FProperty* ConfigProp = Character->GetClass()->FindPropertyByName(FName("CharacterConfig"));
	if (ConfigProp)
	{
		FObjectProperty* ObjectProp = CastField<FObjectProperty>(ConfigProp);
		if (ObjectProp)
		{
			void* PropAddress = ConfigProp->ContainerPtrToValuePtr<void>(Character);
			Config = Cast<UCharacterConfigurationAsset>(ObjectProp->GetObjectPropertyValue(PropAddress));
		}
	}

	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupMovement: CharacterConfig is not set!"));
		return;
	}

	Character->bUseControllerRotationPitch = false;
	Character->bUseControllerRotationYaw = false;
	Character->bUseControllerRotationRoll = false;

	MoveComp->bOrientRotationToMovement = true;
	MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
	MoveComp->RotationRate = Config->RotationRate;
	MoveComp->JumpZVelocity = Config->JumpVelocity;
	MoveComp->AirControl = Config->AirControl;
	MoveComp->MaxWalkSpeed = Config->RunSpeed;
	MoveComp->MinAnalogWalkSpeed = Config->MinAnalogWalkSpeed;
	MoveComp->GroundFriction = Config->GroundFriction;
	MoveComp->BrakingDecelerationWalking = Config->BrakingDecelerationWalking;
	MoveComp->BrakingDecelerationFalling = Config->BrakingDecelerationFalling;
	MoveComp->bUseFlatBaseForFloorChecks = true;
	
	UE_LOG(LogTemp, Log, TEXT("SetupMovement: Movement configured"));
}

void UCharacterSetupComponent::SetupCamera(AMyProjectCharacter* Character)
{
	USpringArmComponent* CameraBoom = Character->GetCameraBoom();
	UCameraComponent* FollowCamera = Character->GetFollowCamera();
	
	if (!CameraBoom || !FollowCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupCamera: Missing camera components!"));
		return;
	}

	// Get config from character
	UCharacterConfigurationAsset* Config = nullptr;
	FProperty* ConfigProp = Character->GetClass()->FindPropertyByName(FName("CharacterConfig"));
	if (ConfigProp)
	{
		FObjectProperty* ObjectProp = CastField<FObjectProperty>(ConfigProp);
		if (ObjectProp)
		{
			void* PropAddress = ConfigProp->ContainerPtrToValuePtr<void>(Character);
			Config = Cast<UCharacterConfigurationAsset>(ObjectProp->GetObjectPropertyValue(PropAddress));
		}
	}

	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupCamera: CharacterConfig is not set!"));
		return;
	}

	CameraBoom->TargetArmLength = Config->CameraDistance;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = Config->bUseCameraLag;
	CameraBoom->bEnableCameraRotationLag = Config->bUseCameraRotationLag;
	if (Config->bUseCameraLag)
	{
		CameraBoom->CameraLagSpeed = Config->CameraLagSpeed;
	}
	CameraBoom->SetRelativeRotation(FRotator(Config->CameraPitch, 0.f, 0.f));

	FollowCamera->bUsePawnControlRotation = false;
	
	UE_LOG(LogTemp, Log, TEXT("SetupCamera: Camera configured"));
}

void UCharacterSetupComponent::SetupProjectileSpawnPoint(AMyProjectCharacter* Character)
{
	// Get projectile spawn point through reflection
	UArrowComponent* ProjectileSpawnPoint = nullptr;
	FProperty* SpawnProp = Character->GetClass()->FindPropertyByName(FName("ProjectileSpawnPoint"));
	if (SpawnProp)
	{
		FObjectProperty* ObjectProp = CastField<FObjectProperty>(SpawnProp);
		if (ObjectProp)
		{
			void* PropAddress = SpawnProp->ContainerPtrToValuePtr<void>(Character);
			ProjectileSpawnPoint = Cast<UArrowComponent>(ObjectProp->GetObjectPropertyValue(PropAddress));
		}
	}

	if (!ProjectileSpawnPoint)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupProjectileSpawnPoint: No spawn point component!"));
		return;
	}

	// Get config from character
	UCharacterConfigurationAsset* Config = nullptr;
	FProperty* ConfigProp = Character->GetClass()->FindPropertyByName(FName("CharacterConfig"));
	if (ConfigProp)
	{
		FObjectProperty* ObjectProp = CastField<FObjectProperty>(ConfigProp);
		if (ObjectProp)
		{
			void* PropAddress = ConfigProp->ContainerPtrToValuePtr<void>(Character);
			Config = Cast<UCharacterConfigurationAsset>(ObjectProp->GetObjectPropertyValue(PropAddress));
		}
	}

	if (Config)
	{
		ProjectileSpawnPoint->SetRelativeLocation(Config->ProjectileSpawnOffset);
		ProjectileSpawnPoint->SetRelativeRotation(Config->ProjectileSpawnRotation);
	}
	else
	{
		// Fallback to defaults
		ProjectileSpawnPoint->SetRelativeLocation(FVector(100.0f, 0.0f, 50.0f));
		ProjectileSpawnPoint->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	}
	
	ProjectileSpawnPoint->bHiddenInGame = false;
	
	UE_LOG(LogTemp, Log, TEXT("SetupProjectileSpawnPoint: Spawn point configured"));
}

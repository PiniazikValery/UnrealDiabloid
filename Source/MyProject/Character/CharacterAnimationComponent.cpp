// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterAnimationComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UCharacterAnimationComponent::UCharacterAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Only tick if needed
}

void UCharacterAnimationComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Cache owner character
	OwnerCharacter = Cast<ACharacter>(GetOwner());
	
	if (!OwnerCharacter.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("UCharacterAnimationComponent: Owner is not a Character!"));
	}
}

void UCharacterAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// Optional: Add any per-frame animation logic here
	// For example, tracking animation state changes
}

void UCharacterAnimationComponent::Initialize()
{
	if (bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCharacterAnimationComponent: Already initialized"));
		return;
	}

	// Validate owner
	if (!OwnerCharacter.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("UCharacterAnimationComponent: Cannot initialize - no valid owner"));
		return;
	}

	// Validate mesh
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("UCharacterAnimationComponent: Cannot initialize - owner has no mesh"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UCharacterAnimationComponent: Initialized successfully"));
	bIsInitialized = true;
}

void UCharacterAnimationComponent::SetAnimationMontages(
	UAnimMontage* InStartFMontage,
	UAnimMontage* InStartRMontage,
	UAnimMontage* InFirstAttackMontage,
	UAnimMontage* InSecondAttackMontage)
{
	StartFMontage = InStartFMontage;
	StartRMontage = InStartRMontage;
	FirstAttackMontage = InFirstAttackMontage;
	SecondAttackMontage = InSecondAttackMontage;

	UE_LOG(LogTemp, Log, TEXT("UCharacterAnimationComponent: Animation montages set"));
}

// ========= ANIMATION PLAYBACK =========

bool UCharacterAnimationComponent::PlayFirstAttack()
{
	if (!FirstAttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCharacterAnimationComponent: FirstAttackMontage is null!"));
		return false;
	}

	return PlayMontageInternal(FirstAttackMontage, 1.0f, NAME_None, TEXT("FirstAttack"));
}

bool UCharacterAnimationComponent::PlaySecondAttack()
{
	if (!SecondAttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCharacterAnimationComponent: SecondAttackMontage is null!"));
		return false;
	}

	return PlayMontageInternal(SecondAttackMontage, 1.0f, NAME_None, TEXT("SecondAttack"));
}

bool UCharacterAnimationComponent::PlayMontage(UAnimMontage* Montage, float PlayRate, FName StartSection)
{
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCharacterAnimationComponent: Montage is null!"));
		return false;
	}

	FName MontageName = Montage->GetFName();
	return PlayMontageInternal(Montage, PlayRate, StartSection, MontageName);
}

bool UCharacterAnimationComponent::PlayMontageInternal(UAnimMontage* Montage, float PlayRate, FName StartSection, FName MontageName)
{
	if (!Montage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("UCharacterAnimationComponent: No AnimInstance found!"));
		return false;
	}

	// Play the montage
	float MontageLength = AnimInstance->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength, 0.0f, true);
	
	if (MontageLength > 0.0f)
	{
		// Jump to section if specified
		if (StartSection != NAME_None)
		{
			AnimInstance->Montage_JumpToSection(StartSection, Montage);
		}

		// Update current montage tracking
		CurrentMontage = Montage;
		CurrentMontageName = MontageName;

		// Setup callbacks for this montage
		SetupMontageCallbacks(Montage, MontageName);

		// Broadcast start event
		OnAnimationStarted.Broadcast(MontageName);

		UE_LOG(LogTemp, Log, TEXT("UCharacterAnimationComponent: Playing montage '%s' (length: %.2f)"), 
			*MontageName.ToString(), MontageLength);

		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("UCharacterAnimationComponent: Failed to play montage '%s'"), 
		*MontageName.ToString());
	return false;
}

void UCharacterAnimationComponent::SetupMontageCallbacks(UAnimMontage* Montage, FName MontageName)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	// Bind to montage end delegate if not already bound
	if (!bHasBoundDelegates)
	{
		AnimInstance->OnMontageEnded.AddDynamic(this, &UCharacterAnimationComponent::HandleMontageEnded);
		AnimInstance->OnMontageBlendingOut.AddDynamic(this, &UCharacterAnimationComponent::HandleMontageBlendOut);
		bHasBoundDelegates = true;
	}
}

void UCharacterAnimationComponent::StopMontage(float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !CurrentMontage)
	{
		return;
	}

	AnimInstance->Montage_Stop(BlendOutTime, CurrentMontage);
	UE_LOG(LogTemp, Log, TEXT("UCharacterAnimationComponent: Stopped montage '%s'"), 
		*CurrentMontageName.ToString());
}

void UCharacterAnimationComponent::StopAllMontages(float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	AnimInstance->Montage_Stop(BlendOutTime);
	CurrentMontage = nullptr;
	CurrentMontageName = NAME_None;
	
	UE_LOG(LogTemp, Log, TEXT("UCharacterAnimationComponent: Stopped all montages"));
}

// ========= ANIMATION STATE QUERIES =========

bool UCharacterAnimationComponent::IsPlayingAnyMontage() const
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	return AnimInstance->IsAnyMontagePlaying();
}

bool UCharacterAnimationComponent::IsPlayingMontage(UAnimMontage* Montage) const
{
	if (!Montage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	return AnimInstance->Montage_IsPlaying(Montage);
}

UAnimMontage* UCharacterAnimationComponent::GetCurrentMontage() const
{
	return CurrentMontage;
}

FName UCharacterAnimationComponent::GetCurrentMontageName() const
{
	return CurrentMontageName;
}

float UCharacterAnimationComponent::GetMontageTimeRemaining() const
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !CurrentMontage)
	{
		return 0.0f;
	}

	// Get current position and calculate remaining time
	float CurrentPos = AnimInstance->Montage_GetPosition(CurrentMontage);
	float PlayLength = CurrentMontage->GetPlayLength();
	return PlayLength - CurrentPos;
}

float UCharacterAnimationComponent::GetMontagePosition() const
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !CurrentMontage)
	{
		return 0.0f;
	}

	return AnimInstance->Montage_GetPosition(CurrentMontage);
}

bool UCharacterAnimationComponent::IsInAttackAnimation() const
{
	return IsPlayingMontage(FirstAttackMontage) || IsPlayingMontage(SecondAttackMontage);
}

// ========= ROOT MOTION =========

void UCharacterAnimationComponent::SetAllowPhysicsRotationDuringRootMotion(bool bAllow)
{
	if (!OwnerCharacter.IsValid())
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if (MovementComp)
	{
		MovementComp->bAllowPhysicsRotationDuringAnimRootMotion = bAllow;
	}
}

bool UCharacterAnimationComponent::GetAllowPhysicsRotationDuringRootMotion() const
{
	if (!OwnerCharacter.IsValid())
	{
		return false;
	}

	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	return MovementComp ? MovementComp->bAllowPhysicsRotationDuringAnimRootMotion : false;
}

// ========= INTERNAL HELPERS =========

USkeletalMeshComponent* UCharacterAnimationComponent::GetOwnerMesh() const
{
	if (!OwnerCharacter.IsValid())
	{
		return nullptr;
	}

	return OwnerCharacter->GetMesh();
}

UAnimInstance* UCharacterAnimationComponent::GetAnimInstance() const
{
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

// ========= EVENT HANDLERS =========

void UCharacterAnimationComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == CurrentMontage)
	{
		UE_LOG(LogTemp, Log, TEXT("UCharacterAnimationComponent: Montage '%s' ended (interrupted: %s)"), 
			*CurrentMontageName.ToString(), 
			bInterrupted ? TEXT("true") : TEXT("false"));

		// Broadcast completion event
		OnAnimationComplete.Broadcast(CurrentMontageName);

		// Clear current montage
		CurrentMontage = nullptr;
		CurrentMontageName = NAME_None;
	}
}

void UCharacterAnimationComponent::HandleMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
	// Optional: Handle blend out differently from end if needed
	UE_LOG(LogTemp, Verbose, TEXT("UCharacterAnimationComponent: Montage '%s' blending out"), 
		*CurrentMontageName.ToString());
}

void UCharacterAnimationComponent::HandleAnimNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	UE_LOG(LogTemp, Log, TEXT("UCharacterAnimationComponent: Anim notify received: %s"), 
		*NotifyName.ToString());

	OnAnimNotifyReceived.Broadcast(NotifyName, BranchingPointPayload);
}

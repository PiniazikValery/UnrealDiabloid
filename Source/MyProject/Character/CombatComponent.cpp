#include "CombatComponent.h"	   // same folder
#include "../MyProjectCharacter.h" // go up one level to reach header in module root
#include "GameFramework/CharacterMovementComponent.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AMyProjectCharacter>(GetOwner());
}

void UCombatComponent::PlayMontage(UAnimMontage* Montage, FOnMontageEnded& Delegate)
{
	if (!OwnerCharacter.IsValid() || !Montage)
		return;
	if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(Montage);
		AnimInstance->Montage_SetEndDelegate(Delegate, Montage);
	}
}

void UCombatComponent::StartDodge()
{
	if (!OwnerCharacter.IsValid() || bIsDodging)
		return;
	OwnerCharacter->SwitchToRunning();
	OwnerCharacter->GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = true;
	bIsDodging = true;

	DodgeMontageDelegate.Unbind();
	DodgeMontageDelegate.BindUObject(this, &UCombatComponent::FinishDodge);
	PlayMontage(OwnerCharacter->GetDodgeMontage(), DodgeMontageDelegate);
}

void UCombatComponent::FinishDodge(UAnimMontage* Montage, bool bInterrupted)
{
	if (!OwnerCharacter.IsValid())
		return;
	bIsDodging = false;
	OwnerCharacter->GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = false;
	FVector Direction = OwnerCharacter->GetActorForwardVector();
	OwnerCharacter->GetCharacterMovement()->Velocity = Direction * 800.f;
}

void UCombatComponent::StartAttack()
{
	if (!OwnerCharacter.IsValid())
		return;
	UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
		return;

	// Cancel momentum if dodging montage currently playing
	if (AnimInstance->Montage_IsPlaying(OwnerCharacter->GetDodgeMontage()))
	{
		FVector Direction = OwnerCharacter->GetActorForwardVector();
		OwnerCharacter->GetCharacterMovement()->Velocity = Direction * 800.f;
	}

	OwnerCharacter->SwitchToWalking();
	AttackMontageDelegate.Unbind();

	if (bIsSecondAttackWindowOpen && !AnimInstance->Montage_IsPlaying(OwnerCharacter->GetSecondAttackMontage()))
	{
		bIsAttacking = true;
		AttackMontageDelegate.BindUObject(this, &UCombatComponent::FinishAttack);
		PlayMontage(OwnerCharacter->GetSecondAttackMontage(), AttackMontageDelegate);
		return;
	}

	if ((!bIsAttacking || bIsAttackEnding) && !AnimInstance->Montage_IsPlaying(OwnerCharacter->GetFirstAttackMontage()))
	{
		bIsAttacking = true;
		AttackMontageDelegate.BindUObject(this, &UCombatComponent::FinishAttack);
		PlayMontage(OwnerCharacter->GetFirstAttackMontage(), AttackMontageDelegate);
	}
}

void UCombatComponent::FinishAttack(UAnimMontage* Montage, bool bInterrupted)
{
	bIsAttacking = false;
	if (OwnerCharacter.IsValid() && !bInterrupted)
	{
		OwnerCharacter->SwitchToRunning();
	}
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, bIsDodging);
	DOREPLIFETIME(UCombatComponent, bIsAttacking);
	DOREPLIFETIME(UCombatComponent, bIsAttackEnding);
	DOREPLIFETIME(UCombatComponent, bIsSecondAttackWindowOpen);
}

// void UCombatComponent::OnRep_SecondAttackWindow()
// {
// 	if (OwnerCharacter.IsValid())
// 	{
// 		UE_LOG(LogTemp, Verbose, TEXT("Second attack window replicated: %s"), bIsSecondAttackWindowOpen ? TEXT("Open") : TEXT("Closed"));
// 	}
// }

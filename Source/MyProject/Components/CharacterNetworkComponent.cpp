// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterNetworkComponent.h"
#include "MyProjectCharacter.h"
#include "Character/CombatComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"

UCharacterNetworkComponent::UCharacterNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	// Enable replication for this component
	SetIsReplicatedByDefault(true);
}

void UCharacterNetworkComponent::BeginPlay()
{
	Super::BeginPlay();
	
	CacheOwnerCharacter();
	
	UE_LOG(LogTemp, Log, TEXT("[%s] CharacterNetworkComponent initialized: HasAuthority=%s, IsLocallyControlled=%s"),
		HasNetworkAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		HasNetworkAuthority() ? TEXT("true") : TEXT("false"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));
}

void UCharacterNetworkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// Replicate movement state
	DOREPLIFETIME(UCharacterNetworkComponent, bIsPlayerTryingToMove);
}

void UCharacterNetworkComponent::CacheOwnerCharacter()
{
	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<AMyProjectCharacter>(GetOwner());
		if (!OwnerCharacter)
		{
			UE_LOG(LogTemp, Error, TEXT("CharacterNetworkComponent: Owner is not AMyProjectCharacter!"));
		}
	}
}

// ========= MOVEMENT RPCs =========

void UCharacterNetworkComponent::SetIsPlayerTryingToMove(bool bNewValue)
{
	if (IsLocallyControlled())
	{
		ServerSetIsPlayerTryingToMove(bNewValue);
	}
}

void UCharacterNetworkComponent::ServerSetIsPlayerTryingToMove_Implementation(bool bNewValue)
{
	bIsPlayerTryingToMove = bNewValue;
}

// ========= COMBAT RPCs =========

void UCharacterNetworkComponent::TriggerAttack(float Angle)
{
	if (!OwnerCharacter)
	{
		CacheOwnerCharacter();
		if (!OwnerCharacter) return;
	}

	if (HasNetworkAuthority())
	{
		// Server: directly multicast
		MulticastStartAttack(Angle);
	}
	else
	{
		// Client: request server to multicast
		ServerStartAttack(Angle);
	}
}

void UCharacterNetworkComponent::ServerStartAttack_Implementation(float Angle)
{
	MulticastStartAttack(Angle);
}

void UCharacterNetworkComponent::MulticastStartAttack_Implementation(float Angle)
{
	if (!OwnerCharacter)
	{
		CacheOwnerCharacter();
		if (!OwnerCharacter) return;
	}

	// Apply rotation and start attack
	OwnerCharacter->SmoothlyRotate(Angle, 1);
	
	if (UCombatComponent* CombatComp = OwnerCharacter->GetCombatComponent())
	{
		CombatComp->StartAttack();
	}
}

void UCharacterNetworkComponent::SetSecondAttackWindow(bool bOpen)
{
	// Only server mutates the CombatComponent state. Only owning client is allowed to ask.
	if (HasNetworkAuthority())
	{
		if (OwnerCharacter)
		{
			if (UCombatComponent* CombatComp = OwnerCharacter->GetCombatComponent())
			{
				CombatComp->SetIsSecondAttackWindowOpen(bOpen);
			}
		}
		return;
	}
	
	// Reject calls from simulated proxies (no owning connection)
	if (!IsLocallyControlled())
	{
		return;
	}
	
	ServerSetSecondAttackWindow(bOpen);
}

void UCharacterNetworkComponent::ServerSetSecondAttackWindow_Implementation(bool bOpen)
{
	if (OwnerCharacter)
	{
		if (UCombatComponent* CombatComp = OwnerCharacter->GetCombatComponent())
		{
			CombatComp->SetIsSecondAttackWindowOpen(bOpen);
		}
	}
}

// ========= DEBUGGING =========

bool UCharacterNetworkComponent::HasNetworkAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

bool UCharacterNetworkComponent::IsLocallyControlled() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->IsLocallyControlled();
	}
	return false;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterNetworkComponent.h"
#include "MyProjectCharacter.h"
#include "Character/CombatComponent.h"
#include "MyCharacterMovementComponent.h"
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
	else if (IsLocallyControlled())
	{
		// Client-side prediction: apply effects locally FIRST for responsive feel
		// This prevents the micro-lag when switching to walking speed
		OwnerCharacter->SmoothlyRotate(Angle, 10);

		// Note: bIgnoreServerCorrections is always true for owning client (set in BeginPlay)

		if (UCombatComponent* CombatComp = OwnerCharacter->GetCombatComponent())
		{
			CombatComp->StartAttack();
		}

		// Then notify server (server will multicast, but we skip re-applying locally)
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

	// Skip for locally controlled clients - they already applied via prediction in TriggerAttack
	// This prevents double-applying the attack which would cause animation/state issues
	if (!HasNetworkAuthority() && IsLocallyControlled())
	{
		return;
	}

	// Apply rotation and start attack for:
	// - Server (authority)
	// - Other clients viewing this character (simulated proxies)
	OwnerCharacter->SmoothlyRotate(Angle, 10);

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

// ========= ATTACK END RPCs =========

void UCharacterNetworkComponent::TriggerAttackEnd()
{
	if (!OwnerCharacter)
	{
		CacheOwnerCharacter();
		if (!OwnerCharacter) return;
	}

	if (HasNetworkAuthority())
	{
		// Server: directly multicast
		MulticastEndAttack();
	}
	else if (IsLocallyControlled())
	{
		// Client-side prediction: apply speed change locally FIRST
		// This prevents the micro-lag when switching back to running speed
		OwnerCharacter->SwitchToRunning();

		// Note: bIgnoreServerCorrections stays true - client is always autonomous
		// (set in BeginPlay, never disabled)

		// Then notify server (server will multicast, but we skip re-applying locally)
		ServerEndAttack();
	}
}

void UCharacterNetworkComponent::ServerEndAttack_Implementation()
{
	MulticastEndAttack();
}

void UCharacterNetworkComponent::MulticastEndAttack_Implementation()
{
	if (!OwnerCharacter)
	{
		CacheOwnerCharacter();
		if (!OwnerCharacter) return;
	}

	// Skip for locally controlled clients - they already applied via prediction in TriggerAttackEnd
	if (!HasNetworkAuthority() && IsLocallyControlled())
	{
		return;
	}

	// Apply speed change for:
	// - Server (authority)
	// - Other clients viewing this character (simulated proxies)
	OwnerCharacter->SwitchToRunning();
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

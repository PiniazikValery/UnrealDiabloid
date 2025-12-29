// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterNetworkComponent.generated.h"

/**
 * UCharacterNetworkComponent
 * 
 * Centralizes all network-related functionality for characters:
 * - RPC functions (Server/Multicast)
 * - Replication logic
 * - Network state management
 * 
 * This component abstracts networking details away from the character class,
 * making it easier to maintain and test network code.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UCharacterNetworkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterNetworkComponent();

	// ========= INITIALIZATION =========
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========= REPLICATED STATE =========
	/** Whether the player is trying to move (replicated for animation sync) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Network|Movement")
	bool bIsPlayerTryingToMove = false;

	// ========= MOVEMENT RPCs =========
	/** Server RPC to update movement state */
	UFUNCTION(Server, Reliable)
	void ServerSetIsPlayerTryingToMove(bool bNewValue);

	/** Helper function to set movement state (handles authority check) */
	UFUNCTION(BlueprintCallable, Category = "Network|Movement")
	void SetIsPlayerTryingToMove(bool bNewValue);

	// ========= COMBAT RPCs =========
	/** Server RPC to initiate attack */
	UFUNCTION(Server, Reliable)
	void ServerStartAttack(float Angle);

	/** Multicast RPC to play attack animation on all clients */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartAttack(float Angle);

	/** Server RPC to update second attack window state */
	UFUNCTION(Server, Reliable)
	void ServerSetSecondAttackWindow(bool bOpen);

	/** Server RPC to end attack */
	UFUNCTION(Server, Reliable)
	void ServerEndAttack();

	/** Multicast RPC to end attack on all clients */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastEndAttack();

	/** Helper function to trigger attack (handles authority/client logic) */
	UFUNCTION(BlueprintCallable, Category = "Network|Combat")
	void TriggerAttack(float Angle);

	/** Helper function to end attack with prediction (handles authority/client logic) */
	UFUNCTION(BlueprintCallable, Category = "Network|Combat")
	void TriggerAttackEnd();

	/** Helper function to set second attack window (handles authority check) */
	UFUNCTION(BlueprintCallable, Category = "Network|Combat")
	void SetSecondAttackWindow(bool bOpen);

	// ========= GETTERS =========
	UFUNCTION(BlueprintPure, Category = "Network|Movement")
	bool GetIsPlayerTryingToMove() const { return bIsPlayerTryingToMove; }

	// ========= DEBUGGING =========
	/** Returns whether this component's owner has authority */
	UFUNCTION(BlueprintPure, Category = "Network|Debug")
	bool HasNetworkAuthority() const;

	/** Returns whether this component's owner is locally controlled */
	UFUNCTION(BlueprintPure, Category = "Network|Debug")
	bool IsLocallyControlled() const;

private:
	/** Cached reference to owning character */
	UPROPERTY()
	class AMyProjectCharacter* OwnerCharacter;

	/** Initialize owner reference */
	void CacheOwnerCharacter();
};

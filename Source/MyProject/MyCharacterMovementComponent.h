#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.generated.h"

// Forward declarations
class UDodge;

UCLASS()
class MYPROJECT_API UMyCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

    /** Is character currently dodging? */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    bool bIsDodging;
    
    /** Direction of dodge (normalized) - replicated for client-server sync */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    FVector DodgeDirection;
    
    /** Cooldown timer - replicated for client-server sync */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    float DodgeCooldownTimer;

public:
    /** Dodge move object for handling dodge functionality */
    UPROPERTY()
    UDodge* DodgeObject;

public:
    //==========================================================================================
    // Functions
    //==========================================================================================
    
    /** Triggers dodge action */
    UFUNCTION(BlueprintCallable, Category = "Dodge")
    void StartDodge();
    
    /** Can character dodge right now? */
    UFUNCTION(BlueprintCallable, Category = "Dodge")
    bool CanDodge() const;
    
    /** Server RPC to start dodge with direction */
    UFUNCTION(Unreliable, Server, WithValidation)
    void ServerStartDodge(const FVector& Direction);

    /** Client RPC to notify dodge state changed */
    UFUNCTION(Reliable, Client)
    void ClientNotifyDodgeStateChanged(bool bNewIsDodging);
    
    /** Client RPC to notify cooldown changed */
    UFUNCTION(Reliable, Client)
    void ClientNotifyCooldownChanged(float NewCooldown);

    /** Multicast RPC to play dodge montage on all clients */
    UFUNCTION(Reliable, NetMulticast)
    void MulticastPlayDodgeMontage();

private:
    /** Play dodge animation montage */
    void PlayDodgeMontage();
    
    /** Disable character rotation during dodge */
    void DisableRotationDuringDodge();
    
    /** Restore character rotation after dodge */
    void RestoreRotationAfterDodge();
    
    /** Rotate character to face dodge direction */
    void RotateToDodgeDirection();
    
    /** Update character rotation based on actual movement direction during dodge */
    void UpdateRotationBasedOnMovement(float DeltaTime);

public:
    //==========================================================================================
    // Overrides for networking
    //==========================================================================================
    
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
    virtual void PhysCustom(float deltaTime, int32 Iterations) override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

    /** Handle replication of dodge-related properties */
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    friend class FSavedMove_MyMovement;
};

//==========================================================================================
// Saved Move Structure
//==========================================================================================

class FSavedMove_MyMovement : public FSavedMove_Character
{
public:
    typedef FSavedMove_Character Super;

    /** Saved dodge state */
    bool bSavedWantsToDodge;
    FVector SavedDodgeDirection;
    float SavedDodgeTimer;
    float SavedDodgeCooldownTimer;

    /** Clear saved variables */
    virtual void Clear() override;
    
    /** Store compressed flags */
    virtual uint8 GetCompressedFlags() const override;
    
    /** Check if moves can be combined */
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
    
    /** Setup move before sending to server */
    virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, 
                            class FNetworkPredictionData_Client_Character& ClientData) override;
    
    /** Prepare move for replaying */
    virtual void PrepMoveFor(class ACharacter* Character) override;
};

//==========================================================================================
// Network Prediction Data
//==========================================================================================

class FNetworkPredictionData_Client_MyMovement : public FNetworkPredictionData_Client_Character
{
public:
    FNetworkPredictionData_Client_MyMovement(const UCharacterMovementComponent& ClientMovement);
    
    typedef FNetworkPredictionData_Client_Character Super;
    
    /** Allocate new saved move */
    virtual FSavedMovePtr AllocateNewMove() override;
};

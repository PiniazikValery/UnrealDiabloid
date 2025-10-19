#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.generated.h"

UCLASS()
class MYPROJECT_API UMyCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

    //==========================================================================================
    // Dodge Properties
    //==========================================================================================
    
    /** Speed of dodge movement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    float DodgeSpeed;
    
    /** Duration of dodge movement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    float DodgeDuration;
    
    /** Cooldown between dodges */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    float DodgeCooldown;
    
    /** Ground dodge speed multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    float GroundDodgeMultiplier;

    /** Small upward velocity during dodge to clear minor ground obstacles while staying grounded */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    float DodgeGroundClearance;

    /** How much to dampen velocity when hitting walls during dodge */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    float DodgeWallSlideFactor;

    /** Dodge animation montage to play */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage;

    /** Is character currently dodging? */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    bool bIsDodging;
    
    /** Wants to perform dodge */
    bool bWantsToDodge;
    
    /** Direction of dodge (normalized) - replicated for client-server sync */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    FVector DodgeDirection;
    
    /** Current dodge timer */
    float DodgeTimer;
    
    /** Cooldown timer - replicated for client-server sync */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    float DodgeCooldownTimer;

private:
    /** Last time we sent ServerStartDodge RPC to prevent spam */
    float LastDodgeRPCTime;
    
    /** Flag to prevent multiple client dodge end predictions */
    bool bClientHasPredictedDodgeEnd;
    
    /** Network sync tracking for dodge timing */
    float LastServerSyncTime;
    bool bWaitingForServerSync;
    float ServerSyncWindow;
    
    /** Saved rotation settings to restore after dodge */
    bool bSavedOrientRotationToMovement;
    bool bSavedUseControllerDesiredRotation;
    FRotator SavedRotationRate;
    
    /** Previous position for tracking actual movement direction during dodge */
    FVector PreviousDodgePosition;
    
    /** Whether we've initialized the previous position for this dodge */
    bool bHasInitializedDodgePosition;

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

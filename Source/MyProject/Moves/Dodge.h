#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"
#include "../MyCharacterMovementComponent.h"
#include "Dodge.generated.h"

/**
 * Dodge move class
 * TODO: Implement dodge functionality
 */
UCLASS()
class MYPROJECT_API UDodge : public UObject
{
	GENERATED_BODY()
	
public:
	UDodge();
	
	// Required for replication
	// virtual bool IsSupportedForNetworking() const override;
	// virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
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
    // UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    // bool bIsDodging;
    
    /** Wants to perform dodge */
    bool bWantsToDodge;
    
    /** Direction of dodge (normalized) - replicated for client-server sync */
    // UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    // FVector DodgeDirection;
    
    /** Current dodge timer */
    float DodgeTimer;
    
    /** Cooldown timer - replicated for client-server sync */
    // UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dodge")
    // float DodgeCooldownTimer;

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
    
    /** Cached reference to the movement component */
    UPROPERTY()
    UMyCharacterMovementComponent* MovementComponent;

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
    void ServerStartDodge(const FVector& Direction);

    /** Set the movement component reference for internal use */
    void SetMovementComponent(UMyCharacterMovementComponent* InMovementComponent);

    /** Client RPC to notify dodge state changed */
    UFUNCTION(Reliable, Client)
    void ClientNotifyDodgeStateChanged(bool bNewIsDodging);
    
    /** Client RPC to notify cooldown changed */
    UFUNCTION(Reliable, Client)
    void ClientNotifyCooldownChanged(float NewCooldown);

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
};

// Physics-based combat component (Dark Souls style dodge + attack state)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimMontage.h"
#include "CombatComponent.generated.h"

class AMyProjectCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCombatComponent();

    // ============= Dodge Tunables =============
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float DodgeDistance = 400.f; // total horizontal distance
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float DodgeDuration = 0.4f; // seconds movement portion lasts
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float DodgeCooldown = 0.5f; // cooldown after movement ends
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float DodgeInvincibilityDuration = 0.3f; // i-frame window from start
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float SteeringInfluence = 0.f; // Currently disabled (server-authoritative path)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float MaxDodgeSpeed = 1500.f; // Maximum allowed dodge speed to prevent flying away
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float MinDodgeSpeed = 100.f; // Minimum dodge speed
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float DodgeAccelerationTime = 0.1f; // Time to reach full speed (0.1s out of 0.4s total)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float DodgeDecelerationTime = 0.15f; // Time to decelerate to stop (0.15s at end)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float VelocityInterpSpeed = 20.f; // How quickly to interpolate to target velocity
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float SmoothingCurveExponent = 2.5f; // Curve exponent for smooth easing (higher = more dramatic)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    bool bAdaptiveSmoothing = true; // Automatically adjust smoothing based on network conditions
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float GoodConnectionThreshold = 50.f; // Ping threshold for "good" connection (ms)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float FastVelocityInterpSpeed = 50.f; // Interpolation speed for good connections
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float FastAccelerationTime = 0.05f; // Acceleration time for good connections
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float PoorConnectionThreshold = 0.5f; // Below this score = poor connection
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float ExtrapolationStrength = 0.8f; // How much to predict ahead for laggy clients
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    bool bUsePositionExtrapolation = true;

    // ============= Replicated Dodge State =============
    UPROPERTY(BlueprintReadOnly, Replicated)
    bool bIsDodging = false;
    UPROPERTY(BlueprintReadOnly, Replicated)
    bool bIsInvincible = false;
    UPROPERTY(ReplicatedUsing=OnRep_DodgeDirection)
    FVector ReplicatedDodgeDirection = FVector::ZeroVector;
    UPROPERTY(ReplicatedUsing=OnRep_DodgeStartTime)
    float DodgeStartTime = 0.f;

    // ============= Attack State (unchanged) =============
    UPROPERTY(BlueprintReadOnly, Replicated)
    bool bIsAttacking = false;
    UPROPERTY(BlueprintReadOnly, Replicated)
    bool bIsAttackEnding = false;
    UPROPERTY(BlueprintReadOnly, Replicated)
    bool bIsSecondAttackWindowOpen = false;

    // ===== Interface =====
    void StartDodge();
    bool CanDodge() const { return !bIsDodging && GetWorld() && GetWorld()->GetTimeSeconds() >= NextDodgeTime; }
    bool GetIsDodging() const { return bIsDodging; }
    bool GetIsInvincible() const { return bIsInvincible; }

    void StartAttack();
    void FinishAttack(class UAnimMontage* Montage, bool bInterrupted);
    void SetIsAttackEnding(bool Value) { bIsAttackEnding = Value; }
    void SetIsSecondAttackWindowOpen(bool Value) { bIsSecondAttackWindowOpen = Value; }
    bool GetIsAttacking() const { return bIsAttacking; }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION(Server, Reliable)
    void ServerStartDodge(FVector DodgeDirection);

    UFUNCTION(Server, Reliable)
    void ServerEndDodge();

    // Network correction functions
    UFUNCTION()
    void OnRep_DodgeDirection();
    
    UFUNCTION()
    void OnRep_DodgeStartTime();

private:
    // Cached owner
    TWeakObjectPtr<AMyProjectCharacter> OwnerCharacter;

    // Timing
    float NextDodgeTime = 0.f;
    float DodgeEndTime = 0.f;
    float InvincibilityEndTime = 0.f;
    FTimerHandle DodgeEndTimerHandle; // Timer to ensure dodge ends when UpdateDodge is disabled

    // Movement cache
    FVector DodgeVelocity = FVector::ZeroVector;
    float OriginalGroundFriction = 0.f;
    float OriginalBrakingDeceleration = 0.f;

    // Network smoothing for poor connections
    FVector LastReplicatedPosition = FVector::ZeroVector;
    FVector PredictedEndPosition = FVector::ZeroVector;
    float LastReplicationTime = 0.f;
    float NetworkSmoothingAlpha = 0.15f; // Lower = smoother but less responsive
    
    // Position history for extrapolation
    TArray<TPair<float, FVector>> PositionHistory;
    static constexpr int32 MaxHistorySize = 10;
    
    // Network quality detection
    float AverageReplicationDelta = 0.016f; // Start with 60fps assumption
    float NetworkQualityScore = 1.0f; // 1.0 = perfect, 0.0 = terrible

    // Internal logic
    void ExecuteDodge(FVector Direction); // Original velocity-based approach (commented out)
    void ExecuteImpulseDodge(FVector Direction); // New impulse-based approach
    void UpdateDodge(float DeltaTime);
    void EndDodge();
    FVector CalculateDodgeDirection() const;
    FVector GetLookDirection() const;
    
    // Smoothing helpers
    float CalculateSmoothDodgeProgress(float Progress) const;
    float CalculateSpeedMultiplier(float Progress) const;
    bool IsGoodConnection() const;
    float GetAdaptiveInterpSpeed() const;
    float GetAdaptiveAccelerationTime() const;
    
    // Network smoothing helpers
    void UpdateDodgeVelocity(float DeltaTime, float CurrentTime, class UCharacterMovementComponent* MoveComp);
    void ApplyNetworkSmoothing(float DeltaTime, float CurrentTime, class UCharacterMovementComponent* MoveComp);
    void ApplyClientSideCorrection(float DeltaTime, float CurrentTime, class UCharacterMovementComponent* MoveComp);
    FVector PredictFutureVelocity(float CurrentProgress, float DeltaTime);
    void UpdatePositionHistory(float Time, const FVector& Position);
    float GetNetworkSmoothingSpeed() const;
    void PreCalculateDodgePath();

    // Attack helpers (existing system)
    FOnMontageEnded AttackMontageDelegate;
    void PlayMontage(class UAnimMontage* Montage, FOnMontageEnded& Delegate);
};

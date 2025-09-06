// Dedicated component handling combat state and montage playback (attack & dodge)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimMontage.h"
#include "CombatComponent.generated.h"

class AMyProjectCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatComponent();

    bool GetIsDodging() const { return bIsDodging; }
    bool GetIsAttacking() const { return bIsAttacking; }
    void SetIsAttackEnding(bool bValue) { bIsAttackEnding = bValue; }
    void SetIsSecondAttackWindowOpen(bool bValue) { bIsSecondAttackWindowOpen = bValue; }
    bool IsSecondAttackWindowOpen() const { return bIsSecondAttackWindowOpen; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Entry points
    void StartDodge();
    void StartAttack();

protected:
    // UActorComponent interface
    virtual void BeginPlay() override; // Needed because we define it in .cpp

    // Internal callbacks
    void FinishDodge(UAnimMontage* Montage, bool bInterrupted);
    void FinishAttack(UAnimMontage* Montage, bool bInterrupted);

private:
    // Owning character cached
    TWeakObjectPtr<AMyProjectCharacter> OwnerCharacter;

    UPROPERTY(Replicated)
    bool bIsDodging = false;
    UPROPERTY(Replicated)
    bool bIsAttacking = false;
    UPROPERTY(Replicated)
    bool bIsAttackEnding = false; // Flag indicating a transition window
    UPROPERTY(Replicated)
    bool bIsSecondAttackWindowOpen = false; // Combo window flag

    // UFUNCTION()
    // void OnRep_SecondAttackWindow();

    // Delegates we reuse to avoid reallocation
    FOnMontageEnded DodgeMontageDelegate;
    FOnMontageEnded AttackMontageDelegate;

    // Helpers
    void PlayMontage(UAnimMontage* Montage, FOnMontageEnded& Delegate);
};

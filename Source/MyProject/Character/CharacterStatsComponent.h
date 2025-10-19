#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatChangedSimple, float, NewValue, float, MaxValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDied);

class AMyProjectCharacter; // forward (optional)

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UCharacterStatsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCharacterStatsComponent();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Replicated core stats
	UPROPERTY(ReplicatedUsing=OnRep_Health, EditAnywhere, BlueprintReadOnly, Category="Stats")
	float Health;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxHealth = 100.f;

	UPROPERTY(ReplicatedUsing=OnRep_Mana, EditAnywhere, BlueprintReadOnly, Category="Stats")
	float Mana;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxMana = 100.f;

	UFUNCTION()
	void OnRep_Health();
	UFUNCTION()
	void OnRep_Mana();

	void BroadcastHealth();
	void BroadcastMana();
	void HandleDeath();

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category="Stats")
	FOnStatChangedSimple OnHealthChanged;
	UPROPERTY(BlueprintAssignable, Category="Stats")
	FOnStatChangedSimple OnManaChanged;
	UPROPERTY(BlueprintAssignable, Category="Stats")
	FOnDied OnDied;

	// Modification API
	UFUNCTION(BlueprintCallable, Category="Stats")
	float ApplyDamage(float DamageAmount);
	UFUNCTION(BlueprintCallable, Category="Stats")
	void Heal(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stats")
	bool SpendMana(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stats")
	void RestoreMana(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stats")
	void SetMaxHealth(float NewMax, bool bClamp = true);
	UFUNCTION(BlueprintCallable, Category="Stats")
	void SetMaxMana(float NewMax, bool bClamp = true);

	// Getters
	UFUNCTION(BlueprintPure, Category="Stats")
	bool IsAlive() const { return Health > 0.f; }
	UFUNCTION(BlueprintPure, Category="Stats")
	float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category="Stats")
	float GetMaxHealth() const { return MaxHealth; }
	UFUNCTION(BlueprintPure, Category="Stats")
	float GetMana() const { return Mana; }
	UFUNCTION(BlueprintPure, Category="Stats")
	float GetMaxMana() const { return MaxMana; }
	UFUNCTION(BlueprintPure, Category="Stats")
	float GetHealthPercent() const { return MaxHealth <= 0.f ? 0.f : Health / MaxHealth; }
	UFUNCTION(BlueprintPure, Category="Stats")
	float GetManaPercent() const { return MaxMana <= 0.f ? 0.f : Mana / MaxMana; }
};

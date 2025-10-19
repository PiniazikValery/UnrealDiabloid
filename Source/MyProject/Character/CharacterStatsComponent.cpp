#include "Character/CharacterStatsComponent.h"
#include "Net/UnrealNetwork.h"

UCharacterStatsComponent::UCharacterStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Health = MaxHealth;
	Mana = MaxMana;
}

void UCharacterStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = FMath::Clamp(Health, 0.f, MaxHealth);
	Mana = FMath::Clamp(Mana, 0.f, MaxMana);
}

void UCharacterStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCharacterStatsComponent, Health);
	DOREPLIFETIME(UCharacterStatsComponent, Mana);
}

void UCharacterStatsComponent::OnRep_Health()
{
	BroadcastHealth();
	if (Health <= 0.f)
	{
		HandleDeath();
	}
}

void UCharacterStatsComponent::OnRep_Mana()
{
	BroadcastMana();
}

void UCharacterStatsComponent::BroadcastHealth()
{
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UCharacterStatsComponent::BroadcastMana()
{
	OnManaChanged.Broadcast(Mana, MaxMana);
}

void UCharacterStatsComponent::HandleDeath()
{
	OnDied.Broadcast();
}

float UCharacterStatsComponent::ApplyDamage(float DamageAmount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || DamageAmount <= 0.f || !IsAlive()) return 0.f;
	const float Old = Health;
	Health = FMath::Clamp(Health - DamageAmount, 0.f, MaxHealth);
	if (Health != Old)
	{
		BroadcastHealth();
		if (Health <= 0.f) HandleDeath();
	}
	return DamageAmount;
}

void UCharacterStatsComponent::Heal(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f || !IsAlive()) return;
	const float Old = Health;
	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
	if (Health != Old) BroadcastHealth();
}

bool UCharacterStatsComponent::SpendMana(float Amount)
{
	if (Amount <= 0.f) return true;
	if (!GetOwner() || !GetOwner()->HasAuthority()) return Mana >= Amount; // predictive success
	if (Mana < Amount) return false;
	Mana -= Amount;
	BroadcastMana();
	return true;
}

void UCharacterStatsComponent::RestoreMana(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f) return;
	const float Old = Mana;
	Mana = FMath::Clamp(Mana + Amount, 0.f, MaxMana);
	if (Mana != Old) BroadcastMana();
}

void UCharacterStatsComponent::SetMaxHealth(float NewMax, bool bClamp)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	MaxHealth = FMath::Max(0.f, NewMax);
	if (bClamp) Health = FMath::Min(Health, MaxHealth);
	BroadcastHealth();
}

void UCharacterStatsComponent::SetMaxMana(float NewMax, bool bClamp)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	MaxMana = FMath::Max(0.f, NewMax);
	if (bClamp) Mana = FMath::Min(Mana, MaxMana);
	BroadcastMana();
}

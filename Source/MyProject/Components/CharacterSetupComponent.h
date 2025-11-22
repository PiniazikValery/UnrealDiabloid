// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterSetupComponent.generated.h"

/**
 * Component responsible for initializing and setting up a character
 * Handles all setup that was previously in Init* methods
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UCharacterSetupComponent : public UActorComponent
{
	GENERATED_BODY()

public:    
	UCharacterSetupComponent();

	/**
	 * Main initialization function - call this in PostInitializeComponents
	 * @param Character - The character to initialize
	 */
	UFUNCTION(BlueprintCallable, Category = "Setup")
	void InitializeCharacter(class AMyProjectCharacter* Character);

protected:
	virtual void BeginPlay() override;

private:
	// These will replace your Init* methods
	void SetupMesh(class AMyProjectCharacter* Character);
	void SetupWeapon(class AMyProjectCharacter* Character);
	void SetupAnimation(class AMyProjectCharacter* Character);
	void SetupAnimationComponent(class AMyProjectCharacter* Character);
	void SetupMovement(class AMyProjectCharacter* Character);
	void SetupCamera(class AMyProjectCharacter* Character);
	void SetupCollision(class AMyProjectCharacter* Character);
	void SetupProjectileSpawnPoint(class AMyProjectCharacter* Character);

	// Validation
	bool ValidateSetup(const class AMyProjectCharacter* Character) const;

	// State tracking
	UPROPERTY()
	bool bIsInitialized = false;

	UPROPERTY()
	TWeakObjectPtr<class AMyProjectCharacter> OwnerCharacter;
};

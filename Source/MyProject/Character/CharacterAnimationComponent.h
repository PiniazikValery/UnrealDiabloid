// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterAnimationComponent.generated.h"

/**
 * Delegate signatures for animation events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnimationComplete, FName, AnimationName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnimationStarted, FName, AnimationName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAnimNotifyReceived, FName, NotifyName, const FBranchingPointNotifyPayload&, BranchingPointPayload);

/**
 * Component responsible for managing all character animations
 * 
 * Responsibilities:
 * - Store animation montage references
 * - Play animation montages
 * - Track animation state
 * - Broadcast animation events
 * - Handle root motion settings
 * 
 * Benefits:
 * - Centralized animation logic
 * - Easy to query animation state
 * - Reusable across different characters
 * - Testable in isolation
 */
UCLASS(ClassGroup=(Animation), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UCharacterAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCharacterAnimationComponent();

	// ========= INITIALIZATION =========
	
	/**
	 * Initialize the component with animation references
	 * Call this in PostInitializeComponents after loading montages
	 */
	void Initialize();

	/**
	 * Set animation montages (can be called from config or manually)
	 */
	void SetAnimationMontages(
		UAnimMontage* InStartFMontage,
		UAnimMontage* InStartRMontage,
		UAnimMontage* InFirstAttackMontage,
		UAnimMontage* InSecondAttackMontage
	);

	// ========= ANIMATION PLAYBACK =========
	
	/**
	 * Play the first attack animation
	 * @return true if animation started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|Combat")
	bool PlayFirstAttack();

	/**
	 * Play the second attack animation (combo)
	 * @return true if animation started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|Combat")
	bool PlaySecondAttack();

	/**
	 * Play a generic montage by reference
	 * @param Montage The montage to play
	 * @param PlayRate Speed multiplier
	 * @param StartSection Optional section to start from
	 * @return true if montage started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	bool PlayMontage(UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSection = NAME_None);

	/**
	 * Stop the currently playing montage
	 * @param BlendOutTime How long to blend out (seconds)
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void StopMontage(float BlendOutTime = 0.25f);

	/**
	 * Stop all montages
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void StopAllMontages(float BlendOutTime = 0.25f);

	// ========= ANIMATION STATE QUERIES =========
	
	/**
	 * Check if any montage is currently playing
	 */
	UFUNCTION(BlueprintPure, Category = "Animation")
	bool IsPlayingAnyMontage() const;

	/**
	 * Check if a specific montage is playing
	 */
	UFUNCTION(BlueprintPure, Category = "Animation")
	bool IsPlayingMontage(UAnimMontage* Montage) const;

	/**
	 * Get the currently playing montage
	 */
	UFUNCTION(BlueprintPure, Category = "Animation")
	UAnimMontage* GetCurrentMontage() const;

	/**
	 * Get the name of the currently playing montage
	 */
	UFUNCTION(BlueprintPure, Category = "Animation")
	FName GetCurrentMontageName() const;

	/**
	 * Get remaining time in current montage
	 */
	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetMontageTimeRemaining() const;

	/**
	 * Get the position (0-1) in the current montage
	 */
	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetMontagePosition() const;

	/**
	 * Check if we're in an attack animation
	 */
	UFUNCTION(BlueprintPure, Category = "Animation|Combat")
	bool IsInAttackAnimation() const;

	// ========= ROOT MOTION =========
	
	/**
	 * Enable/disable root motion for physics rotation
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|RootMotion")
	void SetAllowPhysicsRotationDuringRootMotion(bool bAllow);

	/**
	 * Get current root motion setting
	 */
	UFUNCTION(BlueprintPure, Category = "Animation|RootMotion")
	bool GetAllowPhysicsRotationDuringRootMotion() const;

	// ========= ANIMATION EVENTS =========
	
	/** Broadcast when any animation montage completes */
	UPROPERTY(BlueprintAssignable, Category = "Animation|Events")
	FOnAnimationComplete OnAnimationComplete;

	/** Broadcast when any animation montage starts */
	UPROPERTY(BlueprintAssignable, Category = "Animation|Events")
	FOnAnimationStarted OnAnimationStarted;

	/** Broadcast when an anim notify is triggered */
	UPROPERTY(BlueprintAssignable, Category = "Animation|Events")
	FOnAnimNotifyReceived OnAnimNotifyReceived;

	// ========= PUBLIC MONTAGE REFERENCES =========
	// These can be accessed by other systems if needed
	
	UFUNCTION(BlueprintPure, Category = "Animation|Montages")
	UAnimMontage* GetFirstAttackMontage() const { return FirstAttackMontage; }
	
	UFUNCTION(BlueprintPure, Category = "Animation|Montages")
	UAnimMontage* GetSecondAttackMontage() const { return SecondAttackMontage; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ========= INTERNAL HELPERS =========
	
	/**
	 * Get the skeletal mesh component from owner
	 */
	USkeletalMeshComponent* GetOwnerMesh() const;

	/**
	 * Get animation instance from owner mesh
	 */
	UAnimInstance* GetAnimInstance() const;

	/**
	 * Internal method to play a montage with full control
	 */
	bool PlayMontageInternal(UAnimMontage* Montage, float PlayRate, FName StartSection, FName MontageName);

	/**
	 * Setup montage end delegates when a montage starts
	 */
	void SetupMontageCallbacks(UAnimMontage* Montage, FName MontageName);

	/**
	 * Called when a montage ends (blend out or interruption)
	 */
	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * Called when a montage is blended out
	 */
	UFUNCTION()
	void HandleMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * Called when an anim notify is triggered
	 */
	UFUNCTION()
	void HandleAnimNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	// ========= ANIMATION MONTAGE REFERENCES =========
	// Moved from AMyProjectCharacter
	
	UPROPERTY()
	TObjectPtr<UAnimMontage> StartFMontage;

	UPROPERTY()
	TObjectPtr<UAnimMontage> StartRMontage;

	UPROPERTY()
	TObjectPtr<UAnimMontage> FirstAttackMontage;

	UPROPERTY()
	TObjectPtr<UAnimMontage> SecondAttackMontage;

	// ========= ANIMATION STATE =========
	
	/** Currently playing montage */
	UPROPERTY()
	TObjectPtr<UAnimMontage> CurrentMontage;

	/** Name of currently playing montage for debugging */
	UPROPERTY()
	FName CurrentMontageName;

	/** Cached owner character */
	UPROPERTY()
	TWeakObjectPtr<ACharacter> OwnerCharacter;

	/** Is component initialized */
	bool bIsInitialized = false;

	/** Track if we've bound to montage delegates */
	bool bHasBoundDelegates = false;
};

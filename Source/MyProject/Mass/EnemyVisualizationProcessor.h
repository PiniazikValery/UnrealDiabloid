// EnemyVisualizationProcessor.h
// Hybrid visualization system: Skeletal Mesh (near) + ISM/VAT (far)

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnemyFragments.h"
#include "Animation/EnemyAnimInstance.h"
#include "EnemyVisualizationProcessor.generated.h"

// ============================================================================
// VAT CONFIGURATION
// ============================================================================

USTRUCT(BlueprintType)
struct FVATAnimationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEnemyAnimationState AnimationType = EEnemyAnimationState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 StartFrame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EndFrame = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bLooping = true;
};

USTRUCT(BlueprintType)
struct FVATConfiguration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> PositionTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> NormalTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> VATStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> VATMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TotalFrames = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVATAnimationData> Animations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BoundsScale = 2.0f;
};

// ============================================================================
// SKELETAL MESH POOL ENTRY
// ============================================================================

USTRUCT()
struct FSkeletalMeshPoolEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	UPROPERTY()
	TWeakObjectPtr<UEnemyAnimInstance> AnimInstance;

	FMassEntityHandle AssignedEntity;
	bool bInUse = false;

	bool IsValid() const
	{
		return Actor.IsValid() && SkeletalMeshComponent.IsValid();
	}
};

// ============================================================================
// SKELETAL MESH CANDIDATE (for sorting)
// ============================================================================

struct FSkeletalMeshCandidate
{
	FMassEntityHandle Entity;
	int32 EntityIndex;
	int32 ChunkIndex;
	float Distance;
	FEnemyVisualizationFragment* VisFragment;
	const FTransform* Transform;
	const FEnemyMovementFragment* Movement = nullptr;
    const FEnemyAttackFragment* Attack = nullptr;
    const FEnemyStateFragment* State = nullptr;
	
	bool operator<(const FSkeletalMeshCandidate& Other) const
	{
		return Distance < Other.Distance; // Closer = higher priority
	}
};

// ============================================================================
// MAIN PROCESSOR
// ============================================================================

UCLASS()
class MYPROJECT_API UEnemyVisualizationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UEnemyVisualizationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

public:
	virtual void BeginDestroy() override;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================
public:
	// Distance thresholds
	UPROPERTY(EditAnywhere, Category = "LOD")
	float SkeletalMeshMaxDistance = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "LOD")
	float VATMaxDistance = 8000.0f;

	UPROPERTY(EditAnywhere, Category = "LOD")
	float LODHysteresis = 200.0f;

	// Skeletal mesh settings
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh")
	TSoftObjectPtr<USkeletalMesh> EnemySkeletalMesh;

	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh")
	TSoftClassPtr<UEnemyAnimInstance> AnimationInstanceClass;

	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh")
	int32 SkeletalMeshPoolSize = 50;

	// VAT settings
	UPROPERTY(EditAnywhere, Category = "VAT")
	FVATConfiguration VATConfig;

	// Performance
	UPROPERTY(EditAnywhere, Category = "Performance")
	int32 UpdateFrequency = 1;

	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bEnableVATRendering = true;

	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bCastShadows = false;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugInfo = false;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	float PoolLockDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Distant LOD")
	TSoftObjectPtr<UStaticMesh> SimpleDistantMesh;

	// Material instances for ISM - Element 0 materials
	UPROPERTY(EditAnywhere, Category = "Distant LOD|Materials")
	TSoftObjectPtr<UMaterialInstance> ISM_Material_0_Idle;

	UPROPERTY(EditAnywhere, Category = "Distant LOD|Materials")
	TSoftObjectPtr<UMaterialInstance> ISM_Material_0_Walk;

	// Material instances for ISM - Element 1 materials
	UPROPERTY(EditAnywhere, Category = "Distant LOD|Materials")
	TSoftObjectPtr<UMaterialInstance> ISM_Material_1_Idle;

	UPROPERTY(EditAnywhere, Category = "Distant LOD|Materials")
	TSoftObjectPtr<UMaterialInstance> ISM_Material_1_Walk;

	// ========================================================================
	// RUNTIME STATE
	// ========================================================================
protected:
	FMassEntityQuery EntityQuery;

	UPROPERTY()
	TArray<FSkeletalMeshPoolEntry> SkeletalMeshPool;
	
	TArray<int32> FreeSkeletalMeshIndices;

	UPROPERTY()
	TObjectPtr<AActor> VATVisualizationActor;

	// ISM for IDLE enemies
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> VATISM;

	TArray<int32> FreeVATInstanceIndices;

	// ISM for WALKING enemies
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> VATISM_Walk;

	TArray<int32> FreeVATInstanceIndices_Walk;

	// Track which ISM each entity is currently in (true = walking, false = idle)
	TMap<int32, bool> ISMInstanceIsWalking;

	TArray<FSkeletalMeshCandidate> CachedAllEntities;

	int32 FrameCounter = 0;
	int32 SortCounter = 0;
	FVector CachedCameraLocation = FVector::ZeroVector;

	// Lazy initialization flag
	bool bIsInitialized = false;

	// ========================================================================
	// INITIALIZATION
	// ========================================================================
	void InitializeSkeletalMeshPool(UWorld* World);
	void InitializeVATSystem(UWorld* World);
	void LoadAssets();

	// ========================================================================
	// SKELETAL MESH MANAGEMENT
	// ========================================================================
	int32 AcquireSkeletalMesh(FMassEntityHandle Entity, const FTransform& Transform);
	void ReleaseSkeletalMesh(int32 PoolIndex);
	void UpdateSkeletalMesh(
		int32 PoolIndex, 
		const FTransform& Transform,
		const FEnemyMovementFragment& Movement,
		const FEnemyAttackFragment& Attack,
		const FEnemyStateFragment& State);

	// ========================================================================
	// VAT/ISM MANAGEMENT
	// ========================================================================
	int32 AcquireVATInstance(const FTransform& Transform, const FEnemyVisualizationFragment& VisFragment, bool bIsWalking);
	void ReleaseVATInstance(int32 InstanceIndex, bool bIsWalking);
	void BatchUpdateVATInstances(
		const TArray<FTransform>& Transforms, 
		const TArray<int32>& Indices,
		bool bIsWalking);
	
	// Switch an entity between idle and walk ISM
	void SwitchISMAnimationState(FEnemyVisualizationFragment& VisFragment, const FTransform& Transform, bool bNewIsWalking);

	// ========================================================================
	// RENDER MODE
	// ========================================================================
	EEnemyRenderMode DetermineRenderMode(float Distance, EEnemyRenderMode CurrentMode) const;
	void TransitionRenderMode(
		FMassEntityHandle Entity, 
		FEnemyVisualizationFragment& VisFragment, 
		EEnemyRenderMode NewMode, 
		const FTransform& Transform);

	// ========================================================================
	// VAT HELPERS
	// ========================================================================
	FVector4 CalculateVATCustomData(EEnemyAnimationState AnimState, float AnimTime) const;
	const FVATAnimationData* GetVATAnimationData(EEnemyAnimationState AnimState) const;
	
	// Helper to determine animation state from fragments
	EEnemyAnimationState DetermineAnimationState(
		const FEnemyMovementFragment& Movement,
		const FEnemyAttackFragment& Attack,
		const FEnemyStateFragment& State) const;

	// ========================================================================
	// PUBLIC API
	// ========================================================================
public:
	UFUNCTION(BlueprintCallable, Category = "Visualization")
	void SetVATRenderingEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Visualization")
	void SetLODDistances(float SkeletalMaxDist, float VATMaxDist);

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void GetVisualizationStats(int32& OutSkeletalMeshCount, int32& OutVATInstanceCount) const;
};

// ============================================================================
// CLEANUP OBSERVER
// ============================================================================

UCLASS()
class MYPROJECT_API UEnemyVisualizationCleanupObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UEnemyVisualizationCleanupObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
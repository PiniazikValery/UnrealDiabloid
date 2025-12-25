// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "EnemyFragments.generated.h"

// ============================================================================
// ENUMS FOR VISUALIZATION
// ============================================================================

UENUM(BlueprintType)
enum class EEnemyRenderMode : uint8
{
	None,
	SkeletalMesh,    // Full skeletal mesh with animations
	ISM_VAT,         // Instanced static mesh with Vertex Animation Textures
	Hidden           // Outside render distance
};

// ============================================================================
// FRAGMENTS
// ============================================================================

/**
 * Fragment: Enemy target tracking (player)
 * Stores information about the enemy's current target
 */
USTRUCT()
struct MYPROJECT_API FEnemyTargetFragment : public FMassFragment
{
	GENERATED_BODY()

	// Current target location (usually player position)
	FVector TargetLocation = FVector::ZeroVector;
	
	// Reference to target actor (weak pointer for safety)
	TWeakObjectPtr<AActor> TargetActor = nullptr;
	
	// Distance to target (cached for performance)
	float DistanceToTarget = 0.0f;
};

/**
 * Fragment: Enemy attack behavior
 * Replaces the attack logic from MyAIController
 */
USTRUCT()
struct MYPROJECT_API FEnemyAttackFragment : public FMassFragment
{
	GENERATED_BODY()

	// Timer for attack cooldown
	float TimeSinceLastAttack = 0.0f;
	
	// Attack interval in seconds (equivalent to 1.5f in original code)
	float AttackInterval = 1.5f;
	
	// Attack range in units (equivalent to 150.0f in original code)
	float AttackRange = 150.0f;
	
	// Is enemy currently in attack range?
	bool bIsInAttackRange = false;
	
	// Damage per attack
	float AttackDamage = 0.5f;
	
	// Is currently attacking (for animation)
	bool bIsAttacking = false;
	
	// Attack type index (for different attack animations)
	int32 AttackType = 0;
	
	// Hit reaction pending
	bool bHitPending = false;
	
	// Hit direction for directional hit reactions (-180 to 180)
	float HitDirection = 0.0f;
	
	// Target for look-at (aim offset)
	FVector LookAtTarget = FVector::ZeroVector;
	
	// Has a valid look-at target
	bool bHasLookAtTarget = false;
};

/**
 * Fragment: Enemy movement parameters
 * Replaces CharacterMovement and PathFollowing logic
 */
USTRUCT()
struct MYPROJECT_API FEnemyMovementFragment : public FMassFragment
{
	GENERATED_BODY()

	// Current velocity
	FVector Velocity = FVector::ZeroVector;
	
	// Current acceleration
	FVector Acceleration = FVector::ZeroVector;
	
	// Current facing direction
	FVector FacingDirection = FVector::ForwardVector;
	
	// Maximum speed
	float MaxSpeed = 600.0f;
	
	// Movement speed in units/second
	float MovementSpeed = 250.0f;
	
	// Rotation interpolation speed (like RInterpTo in original code)
	float RotationSpeed = 10.0f;
	
	// How often to recalculate path (equivalent to 0.2f in original)
	float PathUpdateInterval = 0.2f;
	
	// Timer for path updates
	float TimeSinceLastPathUpdate = 0.0f;
	
	// Minimum distance to target before stopping (equivalent to 30.0f)
	float AcceptanceRadius = 30.0f;
	
	// Cached navigation waypoint (updated periodically)
	FVector CachedWaypoint = FVector::ZeroVector;
	
	// Is cached waypoint valid?
	bool bHasValidWaypoint = false;
	
	// Stuck detection counter
	int32 StuckCounter = 0;
	
	// Last position when stuck (for detecting if we escaped)
	FVector LastStuckPosition = FVector::ZeroVector;
	
	// Maximum acceleration (from CharacterMovement settings)
	float MaxAcceleration = 2048.0f;
	
	// Braking deceleration (from CharacterMovement settings)
	float BrakingDeceleration = 2048.0f;

	FVector LastMoveDirection = FVector::ZeroVector;

	// Is currently falling/in air (for animation)
	bool bIsFalling = false;

	// Counter for consecutive pathfinding failures
	int32 PathfindingFailureCount = 0;

	// Should the enemy stop moving (path is blocked or unreachable)
	bool bShouldStop = false;

	int32 BlockedFrameCount = 0;

	FVector CurrentFlankDirection = FVector::ZeroVector;
	float FlankDirectionLockTimer = 0.0f;
	bool bIsCurrentlyFlanking = false;    
	int32 TacticalID = INDEX_NONE;  // ID for tactical planner
	FVector DesiredFacingDirection = FVector::ZeroVector;

	// Slot-based movement system
	int32 AssignedSlotIndex = INDEX_NONE;  // Which slot this enemy is assigned to
	FVector AssignedSlotWorldPosition = FVector::ZeroVector;  // World position of assigned slot
	bool bHasAssignedSlot = false;  // Does this enemy have a slot assigned?
	float SlotReassignmentCooldown = 0.0f;  // Prevent rapid slot switching
	bool bAtSlotPosition = false;  // Has the enemy arrived at its slot? (hysteresis for movement)
};

/**
 * Fragment: Enemy state data
 * Stores runtime state information
 */
USTRUCT()
struct MYPROJECT_API FEnemyStateFragment : public FMassFragment
{
	GENERATED_BODY()

	// Is enemy actively moving?
	bool bIsMoving = false;
	
	// Previous frame location (for movement detection)
	FVector PreviousLocation = FVector::ZeroVector;
	
	// Entity unique ID for debugging
	int32 EntityID = -1;
	
	// Is the enemy alive?
	bool bIsAlive = true;
	
	// Current health
	float Health = 100.0f;
	
	// Maximum health
	float MaxHealth = 100.0f;
};

/**
 * Tag: Identifies entities as enemies (for processor filtering)
 */
USTRUCT()
struct MYPROJECT_API FEnemyTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * Tag: Enemy is in combat
 */
USTRUCT()
struct MYPROJECT_API FEnemyInCombatTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * Tag: Enemy is dead (for cleanup)
 */
USTRUCT()
struct MYPROJECT_API FEnemyDeadTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * Fragment: Links entity to its visual representation
 * Supports both ISM (for VAT) and pooled skeletal meshes
 */
USTRUCT()
struct MYPROJECT_API FEnemyVisualizationFragment : public FMassFragment
{
	GENERATED_BODY()

	// Current render mode
	EEnemyRenderMode RenderMode = EEnemyRenderMode::None;

	// Index of this entity's instance in the ISM component (for VAT rendering)
	// -1 means no instance created yet
	int32 ISMInstanceIndex = -1;

	// Index into the skeletal mesh pool (for skeletal mesh rendering)
	// INDEX_NONE means no skeletal mesh assigned
	int32 SkeletalMeshPoolIndex = INDEX_NONE;

	// Is this entity currently visible?
	bool bIsVisible = true;

	// LOD level (0 = highest detail, 3 = lowest)
	int32 CurrentLOD = 0;

	// Cached distance to camera (updated each frame)
	float CachedDistanceToCamera = 0.0f;

	// Animation time accumulator (for VAT animation)
	float AnimationTime = 0.0f;

	// Animation play rate multiplier
	float AnimationPlayRate = 1.0f;

	UPROPERTY()
	float PoolLockTimer = 0.0f;

	// Track which ISM this entity is in: true = walking ISM, false = idle ISM
	bool bISMIsWalking = false;
};

/**
 * Fragment: Network replication data
 * Contains compressed state for network transmission
 * Used for custom MASS entity replication (MASS entities are NOT UObjects)
 */
USTRUCT()
struct MYPROJECT_API FEnemyNetworkFragment : public FMassFragment
{
	GENERATED_BODY()

	// Network identity (stable ID for client-server entity mapping)
	int32 NetworkID = INDEX_NONE;

	// Replication control
	float TimeSinceLastReplication = 0.0f;     // Tracks when last replicated
	uint8 ReplicationPriority = 0;             // 0-255, higher = more frequent updates
	bool bIsRelevantToAnyClient = false;       // Relevancy flag

	// Compressed state (packed for bandwidth efficiency)
	FVector_NetQuantize10 ReplicatedPosition = FVector::ZeroVector;  // 10cm precision
	uint16 ReplicatedRotationYaw = 0;          // 0-65535 mapped to 0-360 degrees
	uint8 ReplicatedHealth = 255;              // 0-255 (scale from 0-100)
	uint8 ReplicatedFlags = 0;                 // Bit-packed: bIsAlive(1), bIsAttacking(1), bIsMoving(1)

	// Prediction data (for client interpolation)
	FVector_NetQuantize ReplicatedVelocity = FVector::ZeroVector;    // For client prediction
	int16 TargetPlayerIndex = -1;              // Which player this enemy targets (-1 = none)

	// CLIENT-SIDE INTERPOLATION DATA
	FVector PreviousPosition = FVector::ZeroVector;
	FVector TargetPosition = FVector::ZeroVector;
	FVector PreviousVelocity = FVector::ZeroVector;
	FVector TargetVelocity = FVector::ZeroVector;
	float PreviousYaw = 0.0f;
	float TargetYaw = 0.0f;
	float InterpolationAlpha = 1.0f;           // 0 = at previous, 1 = at target
	float TimeSinceLastUpdate = 0.0f;
	float ExpectedUpdateInterval = 0.1f;       // Estimated time between server updates
	bool bHasReceivedFirstUpdate = false;
};
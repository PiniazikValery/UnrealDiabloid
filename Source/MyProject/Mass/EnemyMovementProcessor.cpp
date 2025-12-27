// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyMovementProcessor.h"
#include "EnemySlotManagerSubsystem.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

// Enable detailed logging for movement debugging
#define LOG_MOVEMENT_DETAILS 0
// Enable debug drawing for slot paths (set to 0 to disable) - DISABLED due to crash
#define DEBUG_DRAW_SLOT_PATHS 0
// Enable debug drawing for slot positions (separate from paths, less draw calls)
// DISABLED - causes crash when called from Mass processor due to ComponentsThatNeedEndOfFrameUpdate issue
#define DEBUG_DRAW_SLOTS 0
// Maximum number of enemies to draw debug info for (to prevent performance issues)
#define DEBUG_DRAW_MAX_ENTITIES 5
// How often to draw debug info (every N frames)
#define DEBUG_DRAW_FRAME_INTERVAL 2
// Enable avoidance investigation logs (set to 1 to enable)
#define LOG_AVOIDANCE_INVESTIGATION 1
// Only log for first N entities to avoid spam
#define LOG_AVOIDANCE_MAX_ENTITIES 3

UEnemyMovementProcessor::UEnemyMovementProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UEnemyMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FEnemyTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FEnemyMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FEnemyStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FEnemyTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FEnemyDeadTag>(EMassFragmentPresence::None);
}

void UEnemyMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Debug: Log periodically to confirm processor is running
	static int32 MovementDebugCounter = 0;
	MovementDebugCounter++;
	if (MovementDebugCounter % 60 == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyMovementProcessor::Execute - Frame %d"), MovementDebugCounter);
	}

	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	// Only run on server - client entities are updated via replication
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// Periodically refresh the player list to handle players joining/leaving
	if (CurrentTime - LastPlayerRefreshTime >= PlayerRefreshInterval || CachedPlayerPawns.Num() == 0)
	{
		LastPlayerRefreshTime = CurrentTime;
		CachedPlayerPawns.Empty();

		// Get all player controllers and cache their pawns
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->GetPawn())
			{
				int32 PlayerIndex = 0;
				// Try to get the player index from the local player
				ULocalPlayer* LP = Cast<ULocalPlayer>(PC->Player);
				if (LP)
				{
					PlayerIndex = LP->GetControllerId();
				}
				else
				{
					// For network players, use the order they appear
					PlayerIndex = CachedPlayerPawns.Num();
				}
				CachedPlayerPawns.Add(PlayerIndex, PC->GetPawn());
			}
		}

		if (CachedPlayerPawns.Num() == 0)
		{
			return;
		}
	}

	// Validate cached players (some may have died or disconnected)
	for (auto It = CachedPlayerPawns.CreateIterator(); It; ++It)
	{
		if (!It->Value.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (CachedPlayerPawns.Num() == 0)
	{
		return;
	}

	// Cache slot manager
	if (!CachedSlotManager.IsValid())
	{
		CachedSlotManager = World->GetSubsystem<UEnemySlotManagerSubsystem>();
		if (!CachedSlotManager.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("EnemyMovementProcessor: Failed to get EnemySlotManagerSubsystem!"));
			return;
		}
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	UEnemySlotManagerSubsystem* SlotManager = CachedSlotManager.Get();

	// Build arrays for quick access to player data
	TArray<int32> PlayerIndices;
	TArray<FVector> PlayerLocations;
	TArray<FVector> PlayerForwards;

	for (const auto& Pair : CachedPlayerPawns)
	{
		if (Pair.Value.IsValid())
		{
			APawn* Pawn = Pair.Value.Get();
			PlayerIndices.Add(Pair.Key);
			PlayerLocations.Add(Pawn->GetActorLocation());
			PlayerForwards.Add(Pawn->GetActorForwardVector());

			// Update slot positions for this player
			SlotManager->UpdateSlotPositions(Pair.Key, Pawn->GetActorLocation(), Pawn->GetActorForwardVector());
		}
	}

	const int32 NumPlayers = PlayerIndices.Num();
	if (NumPlayers == 0)
	{
		return;
	}

	// Debug draw slots periodically (not every frame to prevent performance issues)
	// Note: DebugDrawSlots now uses persistent debug lines to avoid ComponentsThatNeedEndOfFrameUpdate crash
#if DEBUG_DRAW_SLOTS
	// Only draw every 5 frames to reduce overhead and prevent any timing issues
	if (FrameCounter % 5 == 0)
	{
		SlotManager->DebugDrawSlots(-1, -1.0f); // PlayerIndex -1 means all players, Duration -1 means one frame
	}
#endif

	FrameCounter++;
	if (EntityQuery.GetEntityManager() != nullptr)
	{
		EntityQuery.ForEachEntityChunk(Context,
			[this, &PlayerIndices, &PlayerLocations, &PlayerForwards, NumPlayers, DeltaTime, World, NavSys, SlotManager, &EntityManager](FMassExecutionContext& Context)
			{
				const auto TransformList = Context.GetMutableFragmentView<FTransformFragment>();
				const auto TargetList = Context.GetMutableFragmentView<FEnemyTargetFragment>();
				const auto MovementList = Context.GetMutableFragmentView<FEnemyMovementFragment>();
				const auto StateList = Context.GetMutableFragmentView<FEnemyStateFragment>();

				const int32 NumEntities = Context.GetNumEntities();

				for (int32 i = 0; i < NumEntities; ++i)
				{
					FTransform& Transform = TransformList[i].GetMutableTransform();
					FEnemyTargetFragment& Target = TargetList[i];
					FEnemyMovementFragment& Movement = MovementList[i];
					FEnemyStateFragment& State = StateList[i];
					FVector& Velocity = Movement.Velocity;
					FMassEntityHandle EntityHandle = Context.GetEntity(i);

					if (!State.bIsAlive)
					{
						// Release slot when enemy dies
						if (Movement.bHasAssignedSlot)
						{
							SlotManager->ReleaseSlotByIndex(Movement.AssignedSlotPlayerIndex, Movement.AssignedSlotIndex);
							Movement.bHasAssignedSlot = false;
							Movement.AssignedSlotIndex = INDEX_NONE;
							Movement.AssignedSlotPlayerIndex = INDEX_NONE;
						}
						continue;
					}

					const FVector CurrentLocation = Transform.GetLocation();

					// =====================================================
					// PLAYER ASSIGNMENT: Assign enemy to nearest player or keep current
					// =====================================================

					// Update player switch cooldown
					if (Target.PlayerSwitchCooldown > 0.0f)
					{
						Target.PlayerSwitchCooldown -= DeltaTime;
					}

					// Find which player this enemy should target
					int32 TargetPlayerArrayIndex = 0;  // Index into our arrays
					int32 TargetPlayerIndex = PlayerIndices[0];  // Actual player index

					if (Target.TargetPlayerIndex != INDEX_NONE && Target.PlayerSwitchCooldown > 0.0f)
					{
						// Already have a target player and cooldown hasn't expired - keep it
						// Find this player in our arrays
						for (int32 p = 0; p < NumPlayers; ++p)
						{
							if (PlayerIndices[p] == Target.TargetPlayerIndex)
							{
								TargetPlayerArrayIndex = p;
								TargetPlayerIndex = Target.TargetPlayerIndex;
								break;
							}
						}
					}
					else
					{
						// Need to assign a player - find the nearest one
						float NearestDistance = FLT_MAX;
						for (int32 p = 0; p < NumPlayers; ++p)
						{
							float DistToPlayer = FVector::Dist(CurrentLocation, PlayerLocations[p]);
							if (DistToPlayer < NearestDistance)
							{
								NearestDistance = DistToPlayer;
								TargetPlayerArrayIndex = p;
								TargetPlayerIndex = PlayerIndices[p];
							}
						}

						// If switching to a different player, release old slot and set cooldown
						if (Target.TargetPlayerIndex != TargetPlayerIndex)
						{
							if (Movement.bHasAssignedSlot && Movement.AssignedSlotPlayerIndex != TargetPlayerIndex)
							{
								SlotManager->ReleaseSlotByIndex(Movement.AssignedSlotPlayerIndex, Movement.AssignedSlotIndex);
								Movement.bHasAssignedSlot = false;
								Movement.AssignedSlotIndex = INDEX_NONE;
								Movement.AssignedSlotPlayerIndex = INDEX_NONE;
								Movement.bAtSlotPosition = false;
							}

							Target.TargetPlayerIndex = TargetPlayerIndex;
							Target.PlayerSwitchCooldown = 3.0f;  // Don't switch players for 3 seconds
						}
					}

					// Get the target player's data
					const FVector PlayerLocation = PlayerLocations[TargetPlayerArrayIndex];
					const FVector PlayerForward = PlayerForwards[TargetPlayerArrayIndex];

					// Get player pawn reference for target
					APawn* TargetPlayerPawn = nullptr;
					if (const TWeakObjectPtr<APawn>* PawnPtr = this->CachedPlayerPawns.Find(TargetPlayerIndex))
					{
						TargetPlayerPawn = PawnPtr->Get();
					}

					// Update target data
					Target.TargetLocation = PlayerLocation;
					Target.TargetActor = TargetPlayerPawn;
					Target.DistanceToTarget = FVector::Dist(CurrentLocation, PlayerLocation);

					const float DistanceToPlayer = Target.DistanceToTarget;

					// =====================================================
					// SLOT ASSIGNMENT: Request or update slot position
					// =====================================================

					// Update slot reassignment cooldown
					if (Movement.SlotReassignmentCooldown > 0.0f)
					{
						Movement.SlotReassignmentCooldown -= DeltaTime;
					}

					// Request a slot if we don't have one or if slot needs refresh
					if (!Movement.bHasAssignedSlot || Movement.SlotReassignmentCooldown <= 0.0f)
					{
						FVector SlotPosition;
						if (SlotManager->RequestSlot(TargetPlayerIndex, EntityHandle, CurrentLocation, SlotPosition))
						{
							// Get the slot index for this entity
							int32 SlotPlayerIdx = INDEX_NONE;
							int32 SlotIdx = INDEX_NONE;
							SlotManager->GetEntitySlot(EntityHandle, SlotPlayerIdx, SlotIdx);

							// Check if we got a different slot than before
							if (Movement.AssignedSlotIndex != SlotIdx || Movement.AssignedSlotPlayerIndex != SlotPlayerIdx)
							{
								Movement.bAtSlotPosition = false;  // Need to move to new slot
							}

							Movement.AssignedSlotPlayerIndex = SlotPlayerIdx;
							Movement.AssignedSlotIndex = SlotIdx;
							Movement.AssignedSlotWorldPosition = SlotPosition;
							Movement.bHasAssignedSlot = true;
							Movement.SlotReassignmentCooldown = 2.0f; // Don't reassign too frequently
						}
						else
						{
							// No slot available - calculate a waiting position at the outer edge
							// instead of targeting player directly (which causes oscillation in crowds)

							// Calculate direction from player to this enemy
							FVector ToEnemy = CurrentLocation - PlayerLocation;
							ToEnemy.Z = 0.0f;
							const float CurrentDistanceToPlayer = ToEnemy.Size();

							if (CurrentDistanceToPlayer > KINDA_SMALL_NUMBER)
							{
								ToEnemy.Normalize();

								// Outer waiting distance - position enemies beyond the outermost slot ring
								// This keeps them out of the way of enemies with slots
								const float OuterWaitingDistance = 450.0f; // Beyond typical slot rings
								const float InnerWaitingDistance = 350.0f; // Minimum waiting distance

								if (CurrentDistanceToPlayer >= InnerWaitingDistance)
								{
									// Already at or beyond waiting distance - stay here and wait
									Movement.AssignedSlotWorldPosition = CurrentLocation;
									Movement.bAtSlotPosition = true; // Mark as arrived so we stop moving
								}
								else
								{
									// Move outward to the waiting zone
									Movement.AssignedSlotWorldPosition = PlayerLocation + ToEnemy * OuterWaitingDistance;
									Movement.bAtSlotPosition = false;
								}
							}
							else
							{
								// Very close to player with no direction - pick a random direction outward
								float RandomAngle = FMath::FRand() * 360.0f;
								FVector RandomDir = FVector::ForwardVector.RotateAngleAxis(RandomAngle, FVector::UpVector);
								Movement.AssignedSlotWorldPosition = PlayerLocation + RandomDir * 450.0f;
								Movement.bAtSlotPosition = false;
							}

							Movement.bHasAssignedSlot = false;
							Movement.SlotReassignmentCooldown = 0.5f; // Try again soon
						}
					}
					else
					{
						if (Movement.bHasAssignedSlot)
						{
							// Update slot world position (it moves with player)
							Movement.AssignedSlotWorldPosition = SlotManager->GetSlotWorldPosition(Movement.AssignedSlotPlayerIndex, Movement.AssignedSlotIndex);

							// Check if our current slot is still on navmesh (player may have moved near a building)
							if (!SlotManager->IsSlotOnNavMesh(Movement.AssignedSlotPlayerIndex, Movement.AssignedSlotIndex))
							{
								// Slot is now off navmesh - release it and request a new one
								SlotManager->ReleaseSlotByIndex(Movement.AssignedSlotPlayerIndex, Movement.AssignedSlotIndex);
								Movement.bHasAssignedSlot = false;
								Movement.bAtSlotPosition = false;
								Movement.AssignedSlotPlayerIndex = INDEX_NONE;
								Movement.SlotReassignmentCooldown = 0.0f; // Request new slot immediately
								continue; // Skip to next entity, will get new slot next frame
							}
						}
						else
						{
							// Slotless enemy - update waiting position to follow the player
							// Calculate direction from player to this enemy
							FVector ToEnemy = CurrentLocation - PlayerLocation;
							ToEnemy.Z = 0.0f;
							const float CurrentDistToPlayer = ToEnemy.Size();

							const float OuterWaitingDistance = 450.0f;
							const float InnerWaitingDistance = 350.0f;
							const float WaitingDriftThreshold = 100.0f; // How far they can drift before resuming movement

							if (CurrentDistToPlayer > KINDA_SMALL_NUMBER)
							{
								ToEnemy.Normalize();

								// Calculate ideal waiting position
								FVector IdealWaitingPos = PlayerLocation + ToEnemy * OuterWaitingDistance;

								if (Movement.bAtSlotPosition)
								{
									// Currently waiting - check if we've drifted too far from ideal
									const float DriftDistance = FVector::Dist2D(CurrentLocation, IdealWaitingPos);

									if (DriftDistance > WaitingDriftThreshold)
									{
										// Drifted too far - need to move to new waiting position
										Movement.AssignedSlotWorldPosition = IdealWaitingPos;
										Movement.bAtSlotPosition = false;
									}
									else if (CurrentDistToPlayer < InnerWaitingDistance - 50.0f)
									{
										// Too close to player (maybe pushed by crowd) - move outward
										Movement.AssignedSlotWorldPosition = IdealWaitingPos;
										Movement.bAtSlotPosition = false;
									}
									else
									{
										// Stay where we are
										Movement.AssignedSlotWorldPosition = CurrentLocation;
									}
								}
								else
								{
									// Not at position yet - update target
									if (CurrentDistToPlayer >= InnerWaitingDistance)
									{
										// Close enough to waiting zone - stop here
										Movement.AssignedSlotWorldPosition = CurrentLocation;
										Movement.bAtSlotPosition = true;
									}
									else
									{
										// Still need to move outward
										Movement.AssignedSlotWorldPosition = IdealWaitingPos;
									}
								}
							}
						}
					}

					// The target for movement is now the slot position, not the player
					const FVector TargetPosition = Movement.AssignedSlotWorldPosition;
					const float DistanceToSlot = FVector::Dist2D(CurrentLocation, TargetPosition);

					// =====================================================
					// MOVEMENT LOGIC
					// =====================================================

					Movement.TimeSinceLastPathUpdate += DeltaTime;

					// Hysteresis for slot arrival:
					// - Arrive when within 50 units
					// - Don't resume movement until 120+ units away (prevents oscillation)
					const float SlotArrivalRadius = 50.0f;
					const float SlotResumeMovementRadius = 120.0f;

#if LOG_AVOIDANCE_INVESTIGATION
					// Log slot status every 60 frames to avoid spam
					if (i < LOG_AVOIDANCE_MAX_ENTITIES && FrameCounter % 60 == 0)
					{
						UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] SLOT STATUS: DistToSlot=%.1f, AtSlot=%d, HasWaypoint=%d, SlotIdx=%d, SlotPos=%s, MyPos=%s"), 
							i, DistanceToSlot, Movement.bAtSlotPosition ? 1 : 0, Movement.bHasValidWaypoint ? 1 : 0,
							Movement.AssignedSlotIndex, *TargetPosition.ToString(), *CurrentLocation.ToString());
					}
#endif

					// Check if we should be "at slot" or "need to move"
					if (Movement.bAtSlotPosition)
					{
						// Already at slot - only start moving if we drifted too far
						if (DistanceToSlot > SlotResumeMovementRadius)
						{
#if LOG_AVOIDANCE_INVESTIGATION
							if (i < LOG_AVOIDANCE_MAX_ENTITIES)
							{
								UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] LEAVING SLOT: Drifted too far, DistToSlot=%.1f > Resume=%.1f"), 
									i, DistanceToSlot, SlotResumeMovementRadius);
							}
#endif
							Movement.bAtSlotPosition = false;
							// Will continue to movement logic below
						}
						else
						{
							// Still at slot - stay idle
							Velocity = FVector::ZeroVector;
							State.bIsMoving = false;
							State.PreviousLocation = CurrentLocation;
							Movement.bShouldStop = false;
							Movement.bHasValidWaypoint = false;

							// Face the player while at slot
							FVector ToPlayer = (PlayerLocation - CurrentLocation);
							ToPlayer.Z = 0.0f;
							if (!ToPlayer.IsNearlyZero())
							{
								Movement.DesiredFacingDirection = ToPlayer.GetSafeNormal();
							}

							// Apply rotation to face player
							if (!Movement.DesiredFacingDirection.IsNearlyZero())
							{
								const FRotator TargetRotation = FRotator(0.0f, Movement.DesiredFacingDirection.Rotation().Yaw, 0.0f);
								const FRotator CurrentRotation = Transform.GetRotation().Rotator();
								const float AngleDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw));

								if (AngleDiff > 3.0f)
								{
									const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 3.0f);
									Transform.SetRotation(NewRotation.Quaternion());
									Movement.FacingDirection = NewRotation.Quaternion().GetForwardVector();
								}
							}
							continue;
						}
					}
					else if (DistanceToSlot <= SlotArrivalRadius)
					{
						// Just arrived at slot
#if LOG_AVOIDANCE_INVESTIGATION
						if (i < LOG_AVOIDANCE_MAX_ENTITIES)
						{
							UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] ARRIVED AT SLOT: DistToSlot=%.1f <= Arrival=%.1f, SlotIdx=%d"), 
								i, DistanceToSlot, SlotArrivalRadius, Movement.AssignedSlotIndex);
						}
#endif
						Movement.bAtSlotPosition = true;
						Velocity = FVector::ZeroVector;
						State.bIsMoving = false;
						State.PreviousLocation = CurrentLocation;
						Movement.bShouldStop = false;
						Movement.bHasValidWaypoint = false;

						// Face the player while at slot
						FVector ToPlayer = (PlayerLocation - CurrentLocation);
						ToPlayer.Z = 0.0f;
						if (!ToPlayer.IsNearlyZero())
						{
							Movement.DesiredFacingDirection = ToPlayer.GetSafeNormal();
						}

						// Apply rotation to face player
						if (!Movement.DesiredFacingDirection.IsNearlyZero())
						{
							const FRotator TargetRotation = FRotator(0.0f, Movement.DesiredFacingDirection.Rotation().Yaw, 0.0f);
							const FRotator CurrentRotation = Transform.GetRotation().Rotator();
							const float AngleDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw));

							if (AngleDiff > 3.0f)
							{
								const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 3.0f);
								Transform.SetRotation(NewRotation.Quaternion());
								Movement.FacingDirection = NewRotation.Quaternion().GetForwardVector();
							}
						}
						continue;
					}

					// =====================================================
					// NAVIGATION: Find path to slot position using NavMesh
					// =====================================================

					bool bNeedsPathUpdate = !Movement.bHasValidWaypoint || Movement.TimeSinceLastPathUpdate >= Movement.PathUpdateInterval;

#if LOG_AVOIDANCE_INVESTIGATION
					// Only log path update when waypoint was actually invalid (not just time-based refresh)
					if (i < LOG_AVOIDANCE_MAX_ENTITIES && bNeedsPathUpdate && !Movement.bHasValidWaypoint)
					{
						UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] PATH UPDATE (waypoint invalid): TimeSinceUpdate=%.3f"), 
							i, Movement.TimeSinceLastPathUpdate);
					}
#endif

					if (bNeedsPathUpdate && NavSys)
					{
						Movement.TimeSinceLastPathUpdate = 0.0f;

						FNavLocation NavStart, NavEnd;
						const FVector SearchExtent(150.0f, 150.0f, 250.0f);

						bool bStartValid = NavSys->ProjectPointToNavigation(CurrentLocation, NavStart, SearchExtent);
						bool bEndValid = NavSys->ProjectPointToNavigation(TargetPosition, NavEnd, SearchExtent);

						if (bStartValid && bEndValid)
						{
							ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();

							if (NavData)
							{
								FPathFindingQuery Query(
									nullptr,
									*NavData,
									NavStart.Location,
									NavEnd.Location
								);

								Query.NavAgentProperties.AgentRadius = 40.0f;
								Query.NavAgentProperties.AgentHeight = 176.0f;

								FPathFindingResult PathResult = NavSys->FindPathSync(Query);

								if (PathResult.IsSuccessful() && PathResult.Path.IsValid())
								{
									const TArray<FNavPathPoint>& PathPoints = PathResult.Path->GetPathPoints();

#if LOG_AVOIDANCE_INVESTIGATION
									if (i < LOG_AVOIDANCE_MAX_ENTITIES)
									{
										UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] PATH FOUND: NumPoints=%d, DistToSlot=%.1f, SlotPos=%s"), 
											i, PathPoints.Num(), DistanceToSlot, *TargetPosition.ToString());
									}
#endif

#if DEBUG_DRAW_SLOT_PATHS
									// Draw the navigation path as a series of connected lines
									// Only draw for first few entities to prevent performance issues
									if (i < DEBUG_DRAW_MAX_ENTITIES && PathPoints.Num() > 1 && FrameCounter % DEBUG_DRAW_FRAME_INTERVAL == 0)
									{
										const float DebugDuration = 0.1f; // Short duration, will be redrawn
										for (int32 PathIdx = 0; PathIdx < PathPoints.Num() - 1; ++PathIdx)
										{
											FVector Start = PathPoints[PathIdx].Location;
											Start.Z += 88.0f; // Offset to capsule center
											FVector End = PathPoints[PathIdx + 1].Location;
											End.Z += 88.0f;
											
											// Color gradient from cyan (start) to magenta (end)
											float T = static_cast<float>(PathIdx) / static_cast<float>(PathPoints.Num() - 1);
											FColor PathColor = FColor(
												static_cast<uint8>(T * 255),
												static_cast<uint8>((1.0f - T) * 255),
												255
											);
											
											DrawDebugLine(World, Start, End, PathColor, false, DebugDuration, 0, 3.0f);
											
											// Draw small spheres at path points
											DrawDebugSphere(World, Start, 15.0f, 6, FColor::Cyan, false, DebugDuration);
										}
										// Draw final point
										FVector FinalPoint = PathPoints.Last().Location;
										FinalPoint.Z += 88.0f;
										DrawDebugSphere(World, FinalPoint, 15.0f, 6, FColor::Magenta, false, DebugDuration);
									}
#endif

									Movement.PathfindingFailureCount = 0;
									Movement.bShouldStop = false;

									if (PathPoints.Num() > 1)
									{
										const float MinWaypointDistance = 100.0f;
										const float MaxHeightDiff = 150.0f;
										int32 NextWaypointIndex = 1;

										for (int32 idx = 1; idx < PathPoints.Num(); ++idx)
										{
											float DistToWaypoint = FVector::Dist2D(CurrentLocation, PathPoints[idx].Location);
											float HeightDiff = FMath::Abs(CurrentLocation.Z - PathPoints[idx].Location.Z - 88.0f);

											if (DistToWaypoint >= MinWaypointDistance)
											{
												if (HeightDiff < MaxHeightDiff)
												{
													NextWaypointIndex = idx;
													break;
												}
											}

											if (idx == PathPoints.Num() - 1)
											{
												NextWaypointIndex = idx;
											}
										}

										Movement.CachedWaypoint = PathPoints[NextWaypointIndex].Location;
										Movement.CachedWaypoint.Z += 88.0f;
										Movement.bHasValidWaypoint = true;

#if LOG_AVOIDANCE_INVESTIGATION
										if (i < LOG_AVOIDANCE_MAX_ENTITIES)
										{
											UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] WAYPOINT SET: WP[%d]=%s, DistToWP=%.1f"), 
												i, NextWaypointIndex, *Movement.CachedWaypoint.ToString(), 
												FVector::Dist2D(CurrentLocation, Movement.CachedWaypoint));
										}
#endif
									}
									else if (PathPoints.Num() == 1)
									{
										Movement.CachedWaypoint = TargetPosition;
										Movement.bHasValidWaypoint = true;
#if LOG_AVOIDANCE_INVESTIGATION
										if (i < LOG_AVOIDANCE_MAX_ENTITIES)
										{
											UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] DIRECT TO SLOT: Only 1 path point, going to slot"), i);
										}
#endif
									}
								}
								else
								{
									Movement.PathfindingFailureCount++;

									const int32 MaxPathfindingFailures = 3;
									if (Movement.PathfindingFailureCount >= MaxPathfindingFailures)
									{
										Movement.bShouldStop = true;
										Movement.bHasValidWaypoint = false;
									}
									else
									{
										Movement.CachedWaypoint = TargetPosition;
										Movement.bHasValidWaypoint = true;
									}
								}
							}
						}
						else
						{
							Movement.CachedWaypoint = TargetPosition;
							Movement.bHasValidWaypoint = true;
						}
					}

					// =====================================================
					// MOVEMENT: Move toward waypoint
					// =====================================================

					if (Movement.bShouldStop)
					{
						Velocity = FVector::ZeroVector;
						State.bIsMoving = false;
						State.PreviousLocation = CurrentLocation;

						if (Movement.TimeSinceLastPathUpdate >= Movement.PathUpdateInterval * 3.0f)
						{
							Movement.bShouldStop = false;
							Movement.PathfindingFailureCount = 0;
							Movement.TimeSinceLastPathUpdate = 0.0f;
						}
						continue;
					}

					if (!Movement.bHasValidWaypoint)
					{
						continue;
					}

					const float BaseMoveDistance = Movement.MovementSpeed * DeltaTime;
					const float MoveDistance = Movement.StuckCounter > 5 ? BaseMoveDistance * 0.5f : BaseMoveDistance;

					FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(40.0f, 88.0f);
					FCollisionQueryParams SweepParams;
					SweepParams.AddIgnoredActor(TargetPlayerPawn);

					FVector DirectionToWaypoint = (Movement.CachedWaypoint - CurrentLocation);
					DirectionToWaypoint.Z = 0.0f;

					const float DistanceToWaypoint = DirectionToWaypoint.Size();

					// When close to waypoint, check if we should also consider ourselves at the slot
					// Reduced threshold from 100 to 50 to get enemies closer to their slots
					if (DistanceToWaypoint < 50.0f)
					{
#if LOG_AVOIDANCE_INVESTIGATION
						if (i < LOG_AVOIDANCE_MAX_ENTITIES)
						{
							UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] REACHED WAYPOINT: DistToWP=%.1f, DistToSlot=%.1f, WP=%s, Slot=%s"), 
								i, DistanceToWaypoint, DistanceToSlot, *Movement.CachedWaypoint.ToString(), *TargetPosition.ToString());
						}
#endif
						Movement.bHasValidWaypoint = false;
						Movement.TimeSinceLastPathUpdate = 999.0f;
						Movement.StuckCounter = 0;
						
						// If we're also close enough to the slot, mark as arrived
						// Use threshold slightly larger than SlotArrivalRadius (50) to prevent oscillation
						if (DistanceToSlot < 60.0f)
						{
#if LOG_AVOIDANCE_INVESTIGATION
							if (i < LOG_AVOIDANCE_MAX_ENTITIES)
							{
								UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] ARRIVED AT SLOT (via waypoint): DistToSlot=%.1f"), 
									i, DistanceToSlot);
							}
#endif
							Movement.bAtSlotPosition = true;
							Velocity = FVector::ZeroVector;
							State.bIsMoving = false;
						}
						continue;
					}

					if (DistanceToWaypoint < KINDA_SMALL_NUMBER)
					{
						continue;
					}

					DirectionToWaypoint.Normalize();

					// =====================================================
					// CROWD AVOIDANCE: Separation from other enemies
					// =====================================================

					FVector SeparationOffset = FVector::ZeroVector;
					int32 NearbyCount = 0;

					const float MinDistance = 85.0f;
					const float DetectionRadius = 160.0f;

					// Slotless enemies in the waiting zone should have reduced avoidance
					// They should stay put and not push into the crowd
					const bool bIsSlotlessWaiting = !Movement.bHasAssignedSlot && Movement.bAtSlotPosition;
					const float AvoidanceMultiplier = bIsSlotlessWaiting ? 0.3f : 1.0f;

					for (int32 j = 0; j < NumEntities; ++j)
					{
						if (i == j)
							continue;
						if (!StateList[j].bIsAlive)
							continue;

						FVector OtherLocation = TransformList[j].GetTransform().GetLocation();
						FVector ToMe = CurrentLocation - OtherLocation;
						ToMe.Z = 0.0f;

						const float Distance = ToMe.Size();

						if (Distance >= DetectionRadius)
							continue;

						if (Distance < KINDA_SMALL_NUMBER)
						{
							float DeterministicAngle = (i * 137.5f);
							FVector EscapeDir = FVector(
								FMath::Cos(FMath::DegreesToRadians(DeterministicAngle)),
								FMath::Sin(FMath::DegreesToRadians(DeterministicAngle)),
								0.0f);
							SeparationOffset += EscapeDir * 2.0f * AvoidanceMultiplier;
							NearbyCount++;
							continue;
						}

						float MyPriority = (i < j) ? 0.3f : 1.0f;
						FVector DirectionAway = ToMe / Distance;

						if (Distance < MinDistance)
						{
							float PenetrationDepth = MinDistance - Distance;
							float OffsetStrength = PenetrationDepth * 0.2f * MyPriority * AvoidanceMultiplier;
							OffsetStrength = FMath::Min(OffsetStrength, 4.0f);
							SeparationOffset += DirectionAway * OffsetStrength;
							NearbyCount++;
						}
						else if (Distance < DetectionRadius)
						{
							float NormalizedDist = (Distance - MinDistance) / (DetectionRadius - MinDistance);
							float OffsetStrength = (1.0f - NormalizedDist) * 0.5f * MyPriority * AvoidanceMultiplier;
							SeparationOffset += DirectionAway * OffsetStrength;
							NearbyCount++;
						}
					}

					if (NearbyCount > 0)
					{
						SeparationOffset /= NearbyCount;
						const float MaxOffsetPerFrame = 4.0f;
						if (SeparationOffset.Size() > MaxOffsetPerFrame)
						{
							SeparationOffset = SeparationOffset.GetSafeNormal() * MaxOffsetPerFrame;
						}
					}

					// =====================================================
					// DIRECTION: Move toward waypoint
					// =====================================================

					FVector DesiredDirection = DirectionToWaypoint;
					FVector FinalDirection;

					if (!Movement.LastMoveDirection.IsNearlyZero())
					{
						const float SmoothingFactor = 3.0f;
						FinalDirection = FMath::VInterpTo(Movement.LastMoveDirection, DesiredDirection, DeltaTime, SmoothingFactor);
						FinalDirection.Normalize();
					}
					else
					{
						FinalDirection = DesiredDirection;
					}

					Movement.LastMoveDirection = FinalDirection;

					// =====================================================
					// CALCULATE FINAL POSITION
					// =====================================================

					FVector DesiredLocation = CurrentLocation + (FinalDirection * MoveDistance);
					DesiredLocation += SeparationOffset;

					// =====================================================
					// COLLISION CHECK
					// =====================================================

					FHitResult SweepHit;
					bool bBlocked = World->SweepSingleByChannel(
						SweepHit,
						CurrentLocation,
						DesiredLocation,
						FQuat::Identity,
						ECC_Pawn,
						CapsuleShape,
						SweepParams);

					FVector NewLocation;

#if LOG_AVOIDANCE_INVESTIGATION
					if (i < LOG_AVOIDANCE_MAX_ENTITIES && bBlocked)
					{
						UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] BLOCKED: HitDist=%.2f, HitActor=%s, HitNormal=%s"), 
							i, SweepHit.Distance, 
							SweepHit.GetActor() ? *SweepHit.GetActor()->GetName() : TEXT("None"),
							*SweepHit.ImpactNormal.ToString());
					}
#endif

					if (bBlocked && SweepHit.Distance < KINDA_SMALL_NUMBER)
					{
						// Inside collision - need to escape
#if LOG_AVOIDANCE_INVESTIGATION
						if (i < LOG_AVOIDANCE_MAX_ENTITIES)
						{
							UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] INSIDE COLLISION - attempting escape"), i);
						}
#endif
						bool bFoundEscape = false;
						FVector EscapeLocation = CurrentLocation;

						if (SweepHit.Component.IsValid())
						{
							FVector PenetrationDir = SweepHit.ImpactNormal;
							if (PenetrationDir.IsNearlyZero() && SweepHit.GetActor())
							{
								PenetrationDir = (CurrentLocation - SweepHit.GetActor()->GetActorLocation());
								PenetrationDir.Z = 0.0f;
								PenetrationDir.Normalize();
							}

							if (!PenetrationDir.IsNearlyZero())
							{
								for (float EscapeDist = 10.0f; EscapeDist <= 200.0f; EscapeDist += 20.0f)
								{
									FVector TestLocation = CurrentLocation + PenetrationDir * EscapeDist;
									TArray<FOverlapResult> Overlaps;
									bool bOverlapping = World->OverlapMultiByChannel(
										Overlaps,
										TestLocation,
										FQuat::Identity,
										ECC_Pawn,
										CapsuleShape,
										SweepParams);

									if (!bOverlapping)
									{
										EscapeLocation = TestLocation;
										bFoundEscape = true;
										break;
									}
								}
							}
						}

						if (!bFoundEscape)
						{
							const int32 NumEscapeDirections = 16;
							const float MaxEscapeDistance = 300.0f;

							for (float EscapeDist = 20.0f; EscapeDist <= MaxEscapeDistance; EscapeDist += 30.0f)
							{
								for (int32 DirIdx = 0; DirIdx < NumEscapeDirections; ++DirIdx)
								{
									float Angle = (360.0f / NumEscapeDirections) * DirIdx;
									FVector EscapeDir = FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector);
									FVector TestLocation = CurrentLocation + EscapeDir * EscapeDist;

									TArray<FOverlapResult> Overlaps;
									bool bOverlapping = World->OverlapMultiByChannel(
										Overlaps,
										TestLocation,
										FQuat::Identity,
										ECC_Pawn,
										CapsuleShape,
										SweepParams);

									if (!bOverlapping)
									{
										if (NavSys)
										{
											FNavLocation NavLoc;
											if (NavSys->ProjectPointToNavigation(TestLocation, NavLoc, FVector(100.0f, 100.0f, 200.0f)))
											{
												EscapeLocation = TestLocation;
												// Extra buffer to prevent immediate re-collision on slopes
												const float SlopeBuffer = 10.0f;
												EscapeLocation.Z = NavLoc.Location.Z + 88.0f + SlopeBuffer;
												bFoundEscape = true;
												break;
											}
										}
										else
										{
											EscapeLocation = TestLocation;
											bFoundEscape = true;
											break;
										}
									}
								}
								if (bFoundEscape)
									break;
							}
						}

						if (bFoundEscape)
						{
							NewLocation = EscapeLocation;
							Velocity = FVector::ZeroVector;
							Movement.StuckCounter = 0;
							Movement.bHasValidWaypoint = false;
							Movement.TimeSinceLastPathUpdate = 999.0f;
							
#if LOG_AVOIDANCE_INVESTIGATION
							if (i < LOG_AVOIDANCE_MAX_ENTITIES)
							{
								UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] ESCAPE SUCCESS: NewLoc=%s"), i, *NewLocation.ToString());
							}
#endif
						}
						else
						{
							Movement.StuckCounter++;
#if LOG_AVOIDANCE_INVESTIGATION
							if (i < LOG_AVOIDANCE_MAX_ENTITIES)
							{
								UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] ESCAPE FAILED: StuckCounter=%d"), i, Movement.StuckCounter);
							}
#endif
							if (Movement.StuckCounter > 500 && NavSys)
							{
								FNavLocation RandomNav;
								if (NavSys->GetRandomReachablePointInRadius(PlayerLocation, 500.0f, RandomNav))
								{
									NewLocation = RandomNav.Location + FVector(0, 0, 88.0f);
									Movement.StuckCounter = 0;
									Movement.bHasValidWaypoint = false;
								}
								else
								{
									NewLocation = CurrentLocation;
								}
							}
							else
							{
								NewLocation = CurrentLocation;
								Velocity = FVector::ZeroVector;
							}
						}
					}
					else if (!bBlocked)
					{
						NewLocation = DesiredLocation;
						FVector TargetVelocity = FinalDirection * Movement.MovementSpeed;
						// Use faster interpolation (8.0) for snappier movement
						Velocity = FMath::VInterpTo(Velocity, TargetVelocity, DeltaTime, 8.0f);
						Movement.StuckCounter = 0;
					}
					else
					{
						// Blocked but not inside collision - check if it's a slope or a wall
						FVector HitNormal = SweepHit.ImpactNormal;
						bool bFoundPath = false;
						
						// Check if this is a slope (high Z component in normal means it's more of a floor than a wall)
						// If Z > 0.7, it's mostly a slope/floor, not a wall - we should walk over it, not slide
						const bool bIsSlope = FMath::Abs(HitNormal.Z) > 0.7f;
						
						FVector SlideDirection;
						const float SlideDistance = MoveDistance;
						
						if (bIsSlope)
						{
							// For slopes, just move in the original horizontal direction
							// The sweep will naturally follow the terrain, and we'll adjust Z after
							// This prevents the "slide into ground" issue
							SlideDirection = FinalDirection;
							SlideDirection.Z = 0.0f;
							if (!SlideDirection.IsNearlyZero())
							{
								SlideDirection.Normalize();
							}

#if LOG_AVOIDANCE_INVESTIGATION
							if (i < LOG_AVOIDANCE_MAX_ENTITIES)
							{
								UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] SLOPE detected: Moving horizontal, Dir=%s"), 
									i, *SlideDirection.ToString());
							}
#endif
						}
						else
						{
							// For walls, slide along the wall surface (original behavior)
							FVector FlatNormal = HitNormal;
							FlatNormal.Z = 0.0f;
							if (!FlatNormal.IsNearlyZero())
							{
								FlatNormal.Normalize();
							}

							// Calculate slide direction (project desired direction onto surface)
							SlideDirection = FinalDirection - (FVector::DotProduct(FinalDirection, FlatNormal) * FlatNormal);
							SlideDirection.Z = 0.0f;
							
							if (!SlideDirection.IsNearlyZero())
							{
								SlideDirection.Normalize();
							}

#if LOG_AVOIDANCE_INVESTIGATION
							if (i < LOG_AVOIDANCE_MAX_ENTITIES)
							{
								UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] WALL detected: HitNormal=%s, SlideDir=%s, SlideDist=%.2f"), 
									i, *FlatNormal.ToString(), *SlideDirection.ToString(), SlideDistance);
							}
#endif
						}

						if (!SlideDirection.IsNearlyZero())
						{
							FVector SlideLocation = CurrentLocation + SlideDirection * SlideDistance;
							
							// For slopes, project to navmesh and skip the sweep (terrain is walkable)
							if (bIsSlope && NavSys)
							{
								FNavLocation NavLoc;
								if (NavSys->ProjectPointToNavigation(SlideLocation, NavLoc, FVector(100.0f, 100.0f, 300.0f)))
								{
									// Use the navmesh Z + capsule half height + extra buffer for slopes
									// The buffer accounts for slope angle causing the capsule to clip the terrain
									const float SlopeBuffer = 10.0f;
									NewLocation = FVector(SlideLocation.X, SlideLocation.Y, NavLoc.Location.Z + 88.0f + SlopeBuffer);
									Velocity = SlideDirection * Movement.MovementSpeed;
									Movement.StuckCounter = 0;
									bFoundPath = true;

#if LOG_AVOIDANCE_INVESTIGATION
									if (i < LOG_AVOIDANCE_MAX_ENTITIES)
									{
										UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] SLOPE move SUCCESS (navmesh): NewLoc=%s, Vel=%.2f"), 
											i, *NewLocation.ToString(), Velocity.Size());
									}
#endif
								}
								else
								{
#if LOG_AVOIDANCE_INVESTIGATION
									if (i < LOG_AVOIDANCE_MAX_ENTITIES)
									{
										UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] SLOPE navmesh projection failed"), i);
									}
#endif
								}
							}
							else
							{
								// For walls, do the normal sweep check
								FHitResult SlideHit;
								bool bSlideBlocked = World->SweepSingleByChannel(
									SlideHit,
									CurrentLocation,
									SlideLocation,
									FQuat::Identity,
									ECC_Pawn,
									CapsuleShape,
									SweepParams);

								if (!bSlideBlocked)
								{
									NewLocation = SlideLocation;
									Velocity = SlideDirection * Movement.MovementSpeed;
									Movement.StuckCounter = 0;
									bFoundPath = true;

#if LOG_AVOIDANCE_INVESTIGATION
									if (i < LOG_AVOIDANCE_MAX_ENTITIES)
									{
										UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] WALL slide SUCCESS: NewLoc=%s, Vel=%.2f"), 
											i, *NewLocation.ToString(), Velocity.Size());
									}
#endif
								}
								else
								{
#if LOG_AVOIDANCE_INVESTIGATION
									if (i < LOG_AVOIDANCE_MAX_ENTITIES)
									{
										UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] WALL slide BLOCKED: SlideHitDist=%.2f"), 
											i, SlideHit.Distance);
									}
#endif
								}
							}
						}

						// If slide failed, try probing alternate directions
						if (!bFoundPath)
						{
							const int32 NumProbeDirections = 16;
							// Use FULL move distance for probing, not reduced
							const float ProbeDistance = MoveDistance;
							float BestScore = -FLT_MAX;
							FVector BestDirection = FVector::ZeroVector;

#if LOG_AVOIDANCE_INVESTIGATION
							if (i < LOG_AVOIDANCE_MAX_ENTITIES)
							{
								UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] Starting probe: NumDirs=%d, ProbeDist=%.2f"), 
									i, NumProbeDirections, ProbeDistance);
							}
#endif

							for (int32 ProbeIdx = 0; ProbeIdx < NumProbeDirections; ++ProbeIdx)
							{
								// Alternate left/right probing from the waypoint direction
								float AngleOffset = (ProbeIdx % 2 == 0 ? 1 : -1) * ((ProbeIdx + 1) / 2) * (360.0f / NumProbeDirections);
								FVector ProbeDir = DirectionToWaypoint.RotateAngleAxis(AngleOffset, FVector::UpVector);
								ProbeDir.Normalize();

								FVector ProbeLocation = CurrentLocation + ProbeDir * ProbeDistance;

								FHitResult ProbeHit;
								bool bProbeBlocked = World->SweepSingleByChannel(
									ProbeHit,
									CurrentLocation,
									ProbeLocation,
									FQuat::Identity,
									ECC_Pawn,
									CapsuleShape,
									SweepParams);

								if (!bProbeBlocked)
								{
									// Score based on how aligned with waypoint direction
									float Score = FVector::DotProduct(ProbeDir, DirectionToWaypoint);
									if (Score > BestScore)
									{
										BestScore = Score;
										BestDirection = ProbeDir;
										bFoundPath = true;
									}
								}
							}

							if (bFoundPath)
							{
								NewLocation = CurrentLocation + BestDirection * ProbeDistance;
								// Use FULL speed when we find a valid probe direction
								Velocity = BestDirection * Movement.MovementSpeed;
								// Don't reduce stuck counter here - we found a path
								Movement.StuckCounter = 0;
								
								// DON'T invalidate waypoint - keep the navigation target

#if LOG_AVOIDANCE_INVESTIGATION
								if (i < LOG_AVOIDANCE_MAX_ENTITIES)
								{
									UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] Probe SUCCESS: BestDir=%s, Score=%.2f, NewLoc=%s"), 
										i, *BestDirection.ToString(), BestScore, *NewLocation.ToString());
								}
#endif
							}
							else
							{
								// Truly stuck - no direction works
								Movement.StuckCounter++;

#if LOG_AVOIDANCE_INVESTIGATION
								if (i < LOG_AVOIDANCE_MAX_ENTITIES)
								{
									UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] ALL PROBES FAILED: StuckCounter=%d"), 
										i, Movement.StuckCounter);
								}
#endif

								// Only request new path after being stuck for a while
								// Increase threshold from 10 to 30 to give more time to unstick naturally
								if (Movement.StuckCounter > 30)
								{
									Movement.bHasValidWaypoint = false;
									Movement.TimeSinceLastPathUpdate = Movement.PathUpdateInterval + 0.1f; // Just slightly over to trigger update
									
#if LOG_AVOIDANCE_INVESTIGATION
									if (i < LOG_AVOIDANCE_MAX_ENTITIES)
									{
										UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] Requesting new path after stuck"), i);
									}
#endif
								}

								NewLocation = CurrentLocation;
								Velocity = FVector::ZeroVector;
							}
						}
					}

					// =====================================================
					// HEIGHT ADJUSTMENT: Keep on NavMesh
					// =====================================================

					if (NavSys && (Movement.StuckCounter == 0 || FrameCounter % 10 == 0))
					{
						FNavLocation ProjectedLoc;
						if (NavSys->ProjectPointToNavigation(NewLocation, ProjectedLoc, FVector(50.0f, 50.0f, 200.0f)))
						{
							// Extra buffer to prevent clipping on slopes
							const float SlopeBuffer = 10.0f;
							float TargetZ = ProjectedLoc.Location.Z + 88.0f + SlopeBuffer;
							NewLocation.Z = FMath::FInterpTo(CurrentLocation.Z, TargetZ, DeltaTime, 5.0f);
						}
					}

					// Apply movement
					Transform.SetLocation(NewLocation);
					State.bIsMoving = !Velocity.IsNearlyZero();

#if DEBUG_DRAW_SLOT_PATHS
					// Draw debug visualization for this enemy's slot assignment
					// Only draw for first few entities and every N frames to prevent crashes
					if (i < DEBUG_DRAW_MAX_ENTITIES && FrameCounter % DEBUG_DRAW_FRAME_INTERVAL == 0)
					{
						const float DebugDuration = 0.1f; // Short duration, will be redrawn
						
						if (Movement.bHasAssignedSlot)
						{
							// Draw line from enemy to its assigned slot
							FColor SlotLineColor = (DistanceToSlot < SlotArrivalRadius) ? FColor::Green : FColor::Orange;
							DrawDebugLine(World, NewLocation, Movement.AssignedSlotWorldPosition, SlotLineColor, false, DebugDuration, 0, 2.0f);
							
							// Draw the assigned slot as a larger sphere
							FColor SlotColor = (DistanceToSlot < SlotArrivalRadius) ? FColor::Green : FColor::Yellow;
							DrawDebugSphere(World, Movement.AssignedSlotWorldPosition, 40.0f, 12, SlotColor, false, DebugDuration, 0, 2.0f);
							
							// Draw slot index above the slot
							DrawDebugString(World, Movement.AssignedSlotWorldPosition + FVector(0, 0, 70), 
								FString::Printf(TEXT("Slot %d"), Movement.AssignedSlotIndex), nullptr, FColor::White, DebugDuration);
							
							// Draw arrow from enemy pointing to waypoint (current navigation target)
							if (Movement.bHasValidWaypoint)
							{
								DrawDebugDirectionalArrow(World, NewLocation + FVector(0, 0, 50), 
									Movement.CachedWaypoint + FVector(0, 0, 50), 50.0f, FColor::Blue, false, DebugDuration, 0, 2.0f);
							}
						}
						else
						{
							// No slot assigned - draw red indicator
							DrawDebugSphere(World, NewLocation + FVector(0, 0, 120), 20.0f, 6, FColor::Red, false, DebugDuration, 0, 2.0f);
						}
					}
#endif

					// Debug logging
					if (i == 0 && MovementDebugCounter % 60 == 0)
					{
						UE_LOG(LogTemp, Warning, TEXT("Entity 0: Slot=%d, SlotPos=%s, Pos=%s, DistToSlot=%.1f"),
							Movement.AssignedSlotIndex, *Movement.AssignedSlotWorldPosition.ToString(),
							*NewLocation.ToString(), DistanceToSlot);
					}

#if LOG_AVOIDANCE_INVESTIGATION
					// Only log frame end when something interesting happened (blocked/stuck)
					if (i < LOG_AVOIDANCE_MAX_ENTITIES && Movement.StuckCounter > 0)
					{
						float MoveDelta = FVector::Dist(CurrentLocation, NewLocation);
						UE_LOG(LogTemp, Warning, TEXT("avoid fix investigate [Entity %d] FRAME END: MoveDelta=%.2f, Velocity=%.2f, IsMoving=%d, StuckCounter=%d"), 
							i, MoveDelta, Velocity.Size(), State.bIsMoving ? 1 : 0, Movement.StuckCounter);
					}
#endif

					Movement.FacingDirection = Transform.GetRotation().GetForwardVector();

					// =====================================================
					// ROTATION: Face movement direction or player
					// =====================================================

					FVector RotationTargetDir = FVector::ZeroVector;

					if (!Movement.DesiredFacingDirection.IsNearlyZero())
					{
						RotationTargetDir = Movement.DesiredFacingDirection;
						Movement.DesiredFacingDirection = FVector::ZeroVector;
					}
					else if (!DirectionToWaypoint.IsNearlyZero() && State.bIsMoving)
					{
						RotationTargetDir = DirectionToWaypoint;
					}
					else if (DistanceToPlayer < 400.0f)
					{
						FVector ToPlayer = (PlayerLocation - CurrentLocation);
						ToPlayer.Z = 0.0f;
						if (!ToPlayer.IsNearlyZero())
						{
							RotationTargetDir = ToPlayer.GetSafeNormal();
						}
					}

					if (!RotationTargetDir.IsNearlyZero())
					{
						RotationTargetDir.Z = 0.0f;
						RotationTargetDir.Normalize();

						const FRotator TargetRotation = FRotator(0.0f, RotationTargetDir.Rotation().Yaw, 0.0f);
						const FRotator CurrentRotation = Transform.GetRotation().Rotator();

						float AngleDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw));

						const float RotationDeadzone = 3.0f;

						if (AngleDiff > RotationDeadzone)
						{
							const float BaseRotationSpeed = 2.5f;
							const float MaxRotationSpeed = 4.0f;

							float AngleScale = FMath::Clamp(AngleDiff / 90.0f, 0.5f, 1.0f);
							float FinalRotationSpeed = FMath::Min(BaseRotationSpeed * AngleScale, MaxRotationSpeed);

							const FRotator NewRotation = FMath::RInterpTo(
								CurrentRotation,
								TargetRotation,
								DeltaTime,
								FinalRotationSpeed);

							Transform.SetRotation(NewRotation.Quaternion());
							Movement.FacingDirection = NewRotation.Quaternion().GetForwardVector();
						}
					}

					State.PreviousLocation = CurrentLocation;
				}
			});
	}
}

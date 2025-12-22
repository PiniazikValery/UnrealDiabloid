// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProject.h"
#include "Modules/ModuleManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EnemyMovementProcessor.h"
#include "Mass/EnemyAttackProcessor.h"

/**
 * Custom Game Module that registers Mass Entity processors
 */
class FMyProjectModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
	}

	virtual void ShutdownModule() override
	{
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMyProjectModule, MyProject, "MyProject");

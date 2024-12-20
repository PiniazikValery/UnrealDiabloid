// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Enums/GestureType.h"
#include "TouchScreenHandler.generated.h"

DECLARE_DELEGATE_OneParam(FCustomTouchDelegate, EGestureType /* TouchType */);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYPROJECT_API UTouchScreenHandler : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UTouchScreenHandler();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void		 BindTouch(EGestureType TouchType, UObject* Object, void (UObject::*Func)(EGestureType));
		
};

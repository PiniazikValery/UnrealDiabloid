// Fill out your copyright notice in the Description page of Project Settings.


#include "./TouchScreenHandler.h"

// Sets default values for this component's properties
UTouchScreenHandler::UTouchScreenHandler()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UTouchScreenHandler::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UTouchScreenHandler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UTouchScreenHandler::BindTouch(EGestureType TouchType, UObject* Object, void (UObject::*Func)(EGestureType))
{
}


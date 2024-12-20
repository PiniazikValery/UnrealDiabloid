// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterInput.generated.h"

class AMyProjectCharacter;
class UInputMappingContext;
struct FInputActionValue;
class UInputAction;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UCharacterInput : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	AMyProjectCharacter*  MyCharacter;
	UInputMappingContext* DefaultMappingContext;
	void				  onTriggeredMove(const FInputActionValue& Value);
	void				  onOngoingMove();
	void				  onNoneMove();
	UInputAction*		  MoveAction;

public:
	UCharacterInput();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void		 SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent, AController* Controller);
};

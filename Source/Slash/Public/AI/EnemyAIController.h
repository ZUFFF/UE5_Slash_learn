// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class UBehaviorTree;
class AEnemy;

UCLASS()
class SLASH_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	static const FName TargetActorKeyName;
	static const FName HomeLocationKeyName;
	static const FName PatrolLocationKeyName;
	static const FName CanAttackKeyName;
	static const FName IsDeadKeyName;

	void StartBehaviorTree(UBehaviorTree* InBehaviorTree, AEnemy* ControlledEnemy);

protected:
	virtual void OnPossess(APawn* InPawn) override;
};


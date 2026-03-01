// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "BTTask_FindPatrolPoint.generated.h"

class UEnvQuery;
class UEnvQueryInstanceBlueprintWrapper;
class UBehaviorTreeComponent;
class AEnemy;

UCLASS()
class SLASH_API UBTTask_FindPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindPatrolPoint();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	UFUNCTION()
	void OnEQSQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus);

	UPROPERTY(EditAnywhere, Category = "EQS")
	UEnvQuery* PatrolPointQuery = nullptr;

private:
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<AEnemy> CachedEnemy;

	UPROPERTY()
	UEnvQueryInstanceBlueprintWrapper* ActiveQueryInstance = nullptr;
};

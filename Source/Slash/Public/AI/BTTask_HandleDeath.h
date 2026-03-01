// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_HandleDeath.generated.h"

UCLASS()
class SLASH_API UBTTask_HandleDeath : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_HandleDeath();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};


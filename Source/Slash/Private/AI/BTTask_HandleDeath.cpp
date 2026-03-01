// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTTask_HandleDeath.h"
#include "AIController.h"
#include "Enemy/Enemy.h"

UBTTask_HandleDeath::UBTTask_HandleDeath()
{
	NodeName = TEXT("HandleDeath");
}

EBTNodeResult::Type UBTTask_HandleDeath::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	if (!AIOwner) return EBTNodeResult::Failed;

	AEnemy* Enemy = Cast<AEnemy>(AIOwner->GetPawn());
	if (!Enemy) return EBTNodeResult::Failed;

	Enemy->ExecuteDeathFromBT();
	return Enemy->IsDeadState() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}


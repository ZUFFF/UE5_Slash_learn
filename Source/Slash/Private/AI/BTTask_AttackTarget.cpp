// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTTask_AttackTarget.h"
#include "AIController.h"
#include "Enemy/Enemy.h"

UBTTask_AttackTarget::UBTTask_AttackTarget()
{
	NodeName = TEXT("AttackTarget");
}

EBTNodeResult::Type UBTTask_AttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	if (!AIOwner) return EBTNodeResult::Failed;

	AEnemy* Enemy = Cast<AEnemy>(AIOwner->GetPawn());
	if (!Enemy) return EBTNodeResult::Failed;
	if (!Enemy->CanAttackFromBT()) return EBTNodeResult::Failed;

	Enemy->ExecuteAttackFromBT();
	return EBTNodeResult::Succeeded;
}


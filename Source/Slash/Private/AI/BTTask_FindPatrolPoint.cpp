// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTTask_FindPatrolPoint.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/EnemyAIController.h"
#include "Enemy/Enemy.h"

UBTTask_FindPatrolPoint::UBTTask_FindPatrolPoint()
{
	NodeName = TEXT("FindPatrolPoint");
}

EBTNodeResult::Type UBTTask_FindPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	if (!AIOwner) return EBTNodeResult::Failed;

	AEnemy* Enemy = Cast<AEnemy>(AIOwner->GetPawn());
	UBlackboardComponent* BBComp = OwnerComp.GetBlackboardComponent();
	if (!Enemy || !BBComp) return EBTNodeResult::Failed;

	AActor* PatrolTarget = Enemy->ChooseNextPatrolTarget();
	if (!PatrolTarget) return EBTNodeResult::Failed;

	Enemy->SetCurrentPatrolTarget(PatrolTarget);
	Enemy->EnterPatrollingStateFromBT();
	BBComp->SetValueAsVector(AEnemyAIController::PatrolLocationKeyName, PatrolTarget->GetActorLocation());
	return EBTNodeResult::Succeeded;
}


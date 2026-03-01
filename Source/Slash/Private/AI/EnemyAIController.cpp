// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/EnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/Enemy.h"

const FName AEnemyAIController::TargetActorKeyName(TEXT("TargetActor"));
const FName AEnemyAIController::HomeLocationKeyName(TEXT("HomeLocation"));
const FName AEnemyAIController::PatrolLocationKeyName(TEXT("PatrolLocation"));
const FName AEnemyAIController::CanAttackKeyName(TEXT("CanAttack"));
const FName AEnemyAIController::IsDeadKeyName(TEXT("IsDead"));

void AEnemyAIController::StartBehaviorTree(UBehaviorTree* InBehaviorTree, AEnemy* ControlledEnemy)
{
	if (!InBehaviorTree || !ControlledEnemy) return;
	if (!InBehaviorTree->BlackboardAsset) return;

	UBlackboardComponent* BBComp = nullptr;
	UseBlackboard(InBehaviorTree->BlackboardAsset, BBComp);
	RunBehaviorTree(InBehaviorTree);

	BBComp = GetBlackboardComponent();
	if (!BBComp) return;

	const FVector SpawnLocation = ControlledEnemy->GetActorLocation();
	BBComp->SetValueAsVector(HomeLocationKeyName, SpawnLocation);
	// Defensive default: avoid PatrolLocation staying at (0,0,0) when query/asset setup is incomplete.
	BBComp->SetValueAsVector(PatrolLocationKeyName, SpawnLocation);
	BBComp->SetValueAsObject(TargetActorKeyName, ControlledEnemy->GetCombatTargetActor());
	BBComp->SetValueAsBool(CanAttackKeyName, false);
	BBComp->SetValueAsBool(IsDeadKeyName, ControlledEnemy->IsDeadState());

	if (AActor* PatrolTarget = ControlledEnemy->GetCurrentPatrolTarget())
	{
		BBComp->SetValueAsVector(PatrolLocationKeyName, PatrolTarget->GetActorLocation());
	}
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

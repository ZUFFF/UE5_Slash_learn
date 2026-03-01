// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTService_UpdateCombatState.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/EnemyAIController.h"
#include "Enemy/Enemy.h"

UBTService_UpdateCombatState::UBTService_UpdateCombatState()
{
	NodeName = TEXT("UpdateCombatState");
	Interval = 0.2f;
	RandomDeviation = 0.0f;
}

void UBTService_UpdateCombatState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	if (!AIOwner) return;

	AEnemy* Enemy = Cast<AEnemy>(AIOwner->GetPawn());
	UBlackboardComponent* BBComp = OwnerComp.GetBlackboardComponent();
	if (!Enemy || !BBComp) return;

	AActor* CombatTarget = Enemy->GetCombatTargetActor();
	if (CombatTarget && Enemy->IsTargetOutsideCombatRangeFromBT())
	{
		Enemy->ClearCombatTargetFromBT();
		CombatTarget = nullptr;
	}

	BBComp->SetValueAsObject(AEnemyAIController::TargetActorKeyName, CombatTarget);
	BBComp->SetValueAsBool(AEnemyAIController::IsDeadKeyName, Enemy->IsDeadState() || Enemy->ShouldDieFromBT());
	BBComp->SetValueAsBool(AEnemyAIController::CanAttackKeyName, Enemy->CanAttackFromBT());
}

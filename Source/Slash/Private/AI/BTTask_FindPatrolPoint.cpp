// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTTask_FindPatrolPoint.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/EnemyAIController.h"
#include "Enemy/Enemy.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"

UBTTask_FindPatrolPoint::UBTTask_FindPatrolPoint()
{
	NodeName = TEXT("FindPatrolPoint");
	bCreateNodeInstance = true;
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_FindPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	if (!AIOwner) return EBTNodeResult::Failed;

	AEnemy* Enemy = Cast<AEnemy>(AIOwner->GetPawn());
	UBlackboardComponent* BBComp = OwnerComp.GetBlackboardComponent();
	if (!Enemy || !BBComp) return EBTNodeResult::Failed;

	// Optional EQS flow: if an EQS asset is provided, use it to choose patrol point.
	if (PatrolPointQuery)
	{
		UEnvQueryInstanceBlueprintWrapper* QueryInstance = UEnvQueryManager::RunEQSQuery(
			Enemy,
			PatrolPointQuery,
			Enemy,
			EEnvQueryRunMode::RandomBest25Pct,
			nullptr
		);

		if (QueryInstance)
		{
			CachedOwnerComp = &OwnerComp;
			CachedEnemy = Enemy;
			ActiveQueryInstance = QueryInstance;
			QueryInstance->GetOnQueryFinishedEvent().AddDynamic(this, &UBTTask_FindPatrolPoint::OnEQSQueryFinished);
			return EBTNodeResult::InProgress;
		}
	}

	// Fallback: keep original random patrol behavior when EQS is not set or query failed to start.
	AActor* PatrolTarget = Enemy->ChooseNextPatrolTarget();
	if (!PatrolTarget) return EBTNodeResult::Failed;

	Enemy->SetCurrentPatrolTarget(PatrolTarget);
	Enemy->EnterPatrollingStateFromBT();
	BBComp->SetValueAsVector(AEnemyAIController::PatrolLocationKeyName, PatrolTarget->GetActorLocation());
	return EBTNodeResult::Succeeded;
}

void UBTTask_FindPatrolPoint::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

	if (ActiveQueryInstance)
	{
		ActiveQueryInstance->GetOnQueryFinishedEvent().RemoveDynamic(this, &UBTTask_FindPatrolPoint::OnEQSQueryFinished);
		ActiveQueryInstance = nullptr;
	}

	CachedOwnerComp.Reset();
	CachedEnemy.Reset();
}

void UBTTask_FindPatrolPoint::OnEQSQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus)
{
	if (QueryInstance != ActiveQueryInstance) return;

	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	AEnemy* Enemy = CachedEnemy.Get();
	ActiveQueryInstance = nullptr;

	if (!OwnerComp || !Enemy)
	{
		CachedOwnerComp.Reset();
		CachedEnemy.Reset();
		return;
	}

	UBlackboardComponent* BBComp = OwnerComp->GetBlackboardComponent();
	if (!BBComp || QueryStatus != EEnvQueryStatus::Success)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[EQS][FindPatrolPoint][%s] Query failed. Status=%d BBValid=%d"),
			*GetNameSafe(Enemy),
			static_cast<int32>(QueryStatus),
			BBComp ? 1 : 0
		);
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		CachedOwnerComp.Reset();
		CachedEnemy.Reset();
		return;
	}

	FVector PatrolLocation = Enemy->GetActorLocation();
	bool bFoundResult = false;

	TArray<AActor*> ResultActors;
	if (QueryInstance->GetQueryResultsAsActors(ResultActors) && ResultActors.Num() > 0 && IsValid(ResultActors[0]))
	{
		Enemy->SetCurrentPatrolTarget(ResultActors[0]);
		PatrolLocation = ResultActors[0]->GetActorLocation();
		bFoundResult = true;
	}
	else
	{
		TArray<FVector> ResultLocations;
		if (QueryInstance->GetQueryResultsAsLocations(ResultLocations) && ResultLocations.Num() > 0)
		{
			Enemy->SetCurrentPatrolTarget(nullptr);
			PatrolLocation = ResultLocations[0];
			bFoundResult = true;
		}
	}

	if (!bFoundResult)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[EQS][FindPatrolPoint][%s] Query success but returned no usable result."),
			*GetNameSafe(Enemy)
		);
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		CachedOwnerComp.Reset();
		CachedEnemy.Reset();
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[EQS][FindPatrolPoint][%s] PatrolLocation=(%.1f, %.1f, %.1f)"),
		*GetNameSafe(Enemy),
		PatrolLocation.X,
		PatrolLocation.Y,
		PatrolLocation.Z
	);

	Enemy->EnterPatrollingStateFromBT();
	BBComp->SetValueAsVector(AEnemyAIController::PatrolLocationKeyName, PatrolLocation);
	FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);

	CachedOwnerComp.Reset();
	CachedEnemy.Reset();
}

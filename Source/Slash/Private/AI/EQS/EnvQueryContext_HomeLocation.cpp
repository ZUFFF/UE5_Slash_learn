// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/EQS/EnvQueryContext_HomeLocation.h"
#include "AI/EnemyAIController.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

void UEnvQueryContext_HomeLocation::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	AAIController* AIController = Cast<AAIController>(QueryOwner);
	APawn* Pawn = nullptr;

	if (AIController)
	{
		Pawn = AIController->GetPawn();
	}
	else
	{
		Pawn = Cast<APawn>(QueryOwner);
		if (Pawn)
		{
			AIController = Cast<AAIController>(Pawn->GetController());
		}
	}

	FVector Origin = FVector::ZeroVector;
	bool bHasOrigin = false;

	if (AIController)
	{
		if (UBlackboardComponent* BBComp = AIController->GetBlackboardComponent())
		{
			Origin = BBComp->GetValueAsVector(AEnemyAIController::HomeLocationKeyName);
			bHasOrigin = true;
		}
	}

	if (!bHasOrigin && Pawn)
	{
		Origin = Pawn->GetActorLocation();
		bHasOrigin = true;
	}

	if (!bHasOrigin)
	{
		if (const AActor* OwnerActor = Cast<AActor>(QueryOwner))
		{
			Origin = OwnerActor->GetActorLocation();
		}
	}

	UEnvQueryItemType_Point::SetContextHelper(ContextData, Origin);
}

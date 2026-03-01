// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_HomeLocation.generated.h"

struct FEnvQueryContextData;
struct FEnvQueryInstance;

/**
 * Provides a stable patrol-search origin from blackboard HomeLocation.
 * Fallback order: Blackboard HomeLocation -> Pawn location -> Actor location.
 */
UCLASS()
class SLASH_API UEnvQueryContext_HomeLocation : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

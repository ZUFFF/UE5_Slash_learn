// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_SlashDamage.generated.h"

/**
 * Damage execution:
 * - Reads SetByCaller "Data.Damage"
 * - Reads source AttackPower
 * - Outputs final value to target IncomingDamage (meta attribute)
 */
UCLASS()
class SLASH_API UExecCalc_SlashDamage : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UExecCalc_SlashDamage();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput
	) const override;
};


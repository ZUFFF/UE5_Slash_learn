// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/GE_SlashDamage.h"

#include "GAS/ExecCalc_SlashDamage.h"

UGE_SlashDamage::UGE_SlashDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UExecCalc_SlashDamage::StaticClass();
	Executions.Add(ExecDef);
}


// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/ExecCalc_SlashDamage.h"

#include "GAS/SlashAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayTagContainer.h"

namespace SlashDamageStatics
{
	static const FGameplayTag DataDamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"), false);

	struct FDamageCaptureDefs
	{
		FGameplayEffectAttributeCaptureDefinition SourceAttackPower;

		FDamageCaptureDefs()
		{
			SourceAttackPower = FGameplayEffectAttributeCaptureDefinition(
				USlashAttributeSet::GetAttackPowerAttribute(),
				EGameplayEffectAttributeCaptureSource::Source,
				true
			);
		}
	};

	const FDamageCaptureDefs& GetCaptureDefs()
	{
		static FDamageCaptureDefs Defs;
		return Defs;
	}
}

UExecCalc_SlashDamage::UExecCalc_SlashDamage()
{
	RelevantAttributesToCapture.Add(SlashDamageStatics::GetCaptureDefs().SourceAttackPower);
}

void UExecCalc_SlashDamage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput
) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = SourceTags;
	EvalParams.TargetTags = TargetTags;

	float SourceAttackPower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		SlashDamageStatics::GetCaptureDefs().SourceAttackPower,
		EvalParams,
		SourceAttackPower
	);

	const float BaseDamage = SlashDamageStatics::DataDamageTag.IsValid()
		? Spec.GetSetByCallerMagnitude(SlashDamageStatics::DataDamageTag, false, 0.f)
		: 0.f;

	// Keep formula simple for now: final = incoming weapon/base damage + source attack power.
	const float FinalDamage = FMath::Max(0.f, BaseDamage + SourceAttackPower);
	if (FinalDamage <= 0.f)
	{
		return;
	}

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		USlashAttributeSet::GetIncomingDamageAttribute(),
		EGameplayModOp::Additive,
		FinalDamage
	));
}


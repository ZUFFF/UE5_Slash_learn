// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/GE_SlashInitAttributes.h"
#include "GAS/SlashAttributeSet.h"

UGE_SlashInitAttributes::UGE_SlashInitAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	auto AddFlatModifier = [this](const FGameplayAttribute& Attribute, float Value)
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = Attribute;
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(Value);
		Modifiers.Add(ModifierInfo);
	};

	AddFlatModifier(USlashAttributeSet::GetMaxHealthAttribute(), 100.f);
	AddFlatModifier(USlashAttributeSet::GetHealthAttribute(), 100.f);
	AddFlatModifier(USlashAttributeSet::GetMaxStaminaAttribute(), 100.f);
	AddFlatModifier(USlashAttributeSet::GetStaminaAttribute(), 100.f);
	AddFlatModifier(USlashAttributeSet::GetAttackPowerAttribute(), 20.f);
}

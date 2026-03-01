// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/GE_SlashAttackCost.h"
#include "GAS/SlashAttributeSet.h"

UGE_SlashAttackCost::UGE_SlashAttackCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo StaminaCostModifier;
	StaminaCostModifier.Attribute = USlashAttributeSet::GetStaminaAttribute();
	StaminaCostModifier.ModifierOp = EGameplayModOp::Additive;
	StaminaCostModifier.ModifierMagnitude = FScalableFloat(-10.f);
	Modifiers.Add(StaminaCostModifier);
}


// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/GE_SlashAttackCooldown.h"
#include "GameplayTagContainer.h"

UGE_SlashAttackCooldown::UGE_SlashAttackCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.35f));

	const FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Attack"), false);
	if (CooldownTag.IsValid())
	{
		InheritableOwnedTagsContainer.AddTag(CooldownTag);
	}
}

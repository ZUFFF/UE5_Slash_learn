// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SlashAttack.generated.h"

/**
 * GAS attack ability:
 * - CommitAbility handles cost/cooldown through GameplayEffects.
 * - Ability body triggers the existing Slash attack authority flow.
 */
UCLASS()
class SLASH_API UGA_SlashAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SlashAttack();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};

// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/GA_SlashAttack.h"
#include "SlashCharacter.h"
#include "GAS/GE_SlashAttackCost.h"
#include "GAS/GE_SlashAttackCooldown.h"
#include "GameplayTagContainer.h"

UGA_SlashAttack::UGA_SlashAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	CostGameplayEffectClass = UGE_SlashAttackCost::StaticClass();
	CooldownGameplayEffectClass = UGE_SlashAttackCooldown::StaticClass();

	const FGameplayTag AbilityAttackTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack"), false);
	if (AbilityAttackTag.IsValid())
	{
		AbilityTags.AddTag(AbilityAttackTag);
	}

	const FGameplayTag CooldownAttackTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Attack"), false);
	if (CooldownAttackTag.IsValid())
	{
		ActivationBlockedTags.AddTag(CooldownAttackTag);
	}
}

void UGA_SlashAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GAS][Attack] CommitAbility failed. Maybe cooldown or stamina cost not satisfied."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ASlashCharacter* SlashCharacter = Cast<ASlashCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		SlashCharacter->TriggerAttackFromAbility();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "SlashPlayerState.h"

#include "AbilitySystemComponent.h"
#include "GAS/SlashAttributeSet.h"

ASlashPlayerState::ASlashPlayerState()
{
	NetUpdateFrequency = 100.f;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	SlashAttributeSet = CreateDefaultSubobject<USlashAttributeSet>(TEXT("SlashAttributeSet"));
}

UAbilitySystemComponent* ASlashPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}


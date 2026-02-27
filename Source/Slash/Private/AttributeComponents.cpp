// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeComponents.h"
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UAttributeComponents::UAttributeComponents()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// ...
}


// Called when the game starts
void UAttributeComponents::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UAttributeComponents::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAttributeComponents, Health);
	DOREPLIFETIME(UAttributeComponents, MaxHealth);
	DOREPLIFETIME(UAttributeComponents, Stamina);
	DOREPLIFETIME(UAttributeComponents, MaxStamina);
	DOREPLIFETIME(UAttributeComponents, Gold);
	DOREPLIFETIME(UAttributeComponents, Souls);
}


// Called every frame
void UAttributeComponents::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAttributeComponents::RegenStamina(float DeltaTime)
{
	Stamina = FMath::Clamp(Stamina + StaminaRegenRate * DeltaTime, 0.f, MaxStamina);
}

void UAttributeComponents::ReceiveDamage(float Damage)
{
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
}

void UAttributeComponents::UseStamina(float StaminaCost)
{
	Stamina = FMath::Clamp(Stamina - StaminaCost, 0.f, MaxStamina);
}

float UAttributeComponents::GetHealthPercent()
{
	return Health / MaxHealth;
}

float UAttributeComponents::GetStaminaPercent()
{
	return Stamina / MaxStamina;
}

bool UAttributeComponents::IsAlive()
{
	return Health>0.f;
}

void UAttributeComponents::AddSouls(int32 NumberOfSouls)
{
	Souls += NumberOfSouls;
}

void UAttributeComponents::AddGold(int32 AmountOfGold)
{
	Gold += AmountOfGold;
}

void UAttributeComponents::OnRep_Health(float LastHealth)
{
	UE_LOG(LogTemp, Warning, TEXT("[属性同步][%s] 生命 %.2f -> %.2f"),
		*GetNameSafe(GetOwner()), LastHealth, Health);
}

void UAttributeComponents::OnRep_Stamina(float LastStamina)
{
	UE_LOG(LogTemp, Warning, TEXT("[属性同步][%s] 体力 %.2f -> %.2f"),
		*GetNameSafe(GetOwner()), LastStamina, Stamina);
}

void UAttributeComponents::OnRep_Gold(int32 LastGold)
{
	UE_LOG(LogTemp, Warning, TEXT("[属性同步][%s] 金币 %d -> %d"),
		*GetNameSafe(GetOwner()), LastGold, Gold);
}

void UAttributeComponents::OnRep_Souls(int32 LastSouls)
{
	UE_LOG(LogTemp, Warning, TEXT("[属性同步][%s] 灵魂 %d -> %d"),
		*GetNameSafe(GetOwner()), LastSouls, Souls);
}


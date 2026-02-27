// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponents.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SLASH_API UAttributeComponents : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAttributeComponents();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void RegenStamina(float DeltaTime);

private:
	// Current Health
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Health, Category = "Actor Attributes")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, Replicated, Category = "Actor Attributes")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Stamina, Category = "Actor Attributes")
	float Stamina = 100.f;

	UPROPERTY(EditAnywhere, Replicated, Category = "Actor Attributes")
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Gold, Category = "Actor Attributes")
	int32 Gold = 0;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Souls, Category = "Actor Attributes")
	int32 Souls = 0;

	UPROPERTY(EditAnywhere, Category = "Actor Attributes")
	int32 DodgeCost = 14.f;

	UPROPERTY(EditAnywhere, Category = "Actor Attributes")
	float StaminaRegenRate = 8.f;

public:
	void ReceiveDamage(float Damage);
	void UseStamina(float StaminaCost);
	float GetHealthPercent();
	float GetStaminaPercent();
		
	bool IsAlive();
	void AddSouls(int32 NumberOfSouls);
	void AddGold(int32 AmountOfGold);
	FORCEINLINE int32 GetGold() const { return Gold; }
	FORCEINLINE int32 GetSouls() const { return Souls; }
	FORCEINLINE int32 GetDodgeCost() const { return DodgeCost; }
	FORCEINLINE int32 GetStamina() const { return Stamina; }

private:
	UFUNCTION()
	void OnRep_Health(float LastHealth);

	UFUNCTION()
	void OnRep_Stamina(float LastStamina);

	UFUNCTION()
	void OnRep_Gold(int32 LastGold);

	UFUNCTION()
	void OnRep_Souls(int32 LastSouls);
};

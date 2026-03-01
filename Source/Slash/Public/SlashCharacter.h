// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "AbilitySystemInterface.h"
#include "CharacterTypes.h"
#include "BaseCharacter.h"
#include "Interfaces/PickupInterface.h"
#include "SlashCharacter.generated.h"

constexpr auto TRACE_LENGTH = 80000;

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UGroomComponent;
class AMyItem;
class UAnimMontage;
class USlashOverlap;
class ASoul;
class ATreasure;
class AEnemy;
class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class USlashAttributeSet;

UCLASS()
class SLASH_API ASlashCharacter : public ABaseCharacter, public IPickupInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASlashCharacter();

	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Jump() override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	FORCEINLINE ECharacterState GetCharacterState() const { return CharacterState; }

	FORCEINLINE EActionState GetActionState() const { return ActionState; }

	FORCEINLINE ECharacterMoveState GetMoveState() const { return MoveState; }

	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }

	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* Hitter) override;

	virtual void SetOverlappingItem(AMyItem* Item) override;
	virtual void AddSouls(ASoul* Soul) override;
	virtual void AddGold(ATreasure* Treasure) override;
	void AimOffset(float DeltaTime);
	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }

	// ��ʼͣ�ٶ���
	UFUNCTION(BlueprintCallable)
	void StartHitReaction();

	// �ָ�����
	void ResumeAction();

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void InitializeAbilitySystem();

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void GrantStartupAbilities();

	UFUNCTION(BlueprintCallable, Category = "GAS")
	bool TryActivateAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass);

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void TriggerAttackFromAbility();

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void ApplyStartupEffects();
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputMappingContext* SlashContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* MovementAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* EKeyAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* DodgeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* LockAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* AimStartAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* AimEndAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* WallRunAction;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void EKeyPressed(const FInputActionValue& Value);
	void Dodge(const FInputActionValue& Value);
	void ChangeToLockedControl();
	void ChangeToUnlockedControl();
	void Lock(const FInputActionValue& Value);
	void AimButtonPressed(const FInputActionValue& Value);
	void AimButtonReleased(const FInputActionValue& Value);
	virtual void Attack(const FInputActionValue& Value) override;
	void WallRun(const FInputActionValue& Value);
	void DrawArrow();
	virtual bool CanAttack() override;
	bool CanShoot();


	virtual void AttackEnd() override;
	virtual void DodgeEnd() override;

	UFUNCTION(BlueprintCallable)
	void AttackFinish();

	UFUNCTION(BlueprintCallable)
	void ShootEnd();

	UFUNCTION(BlueprintCallable)
	void AttachArrow();

	UFUNCTION(BlueprintCallable)
	void DrawEnd();
	
	UFUNCTION(Server, Reliable)
	void ServerRequestAttack();

	UFUNCTION(Server, Reliable)
	void ServerInteract();

	UFUNCTION(Server, Reliable)
	void ServerRequestDodge();

	UFUNCTION(Server, Reliable)
	void ServerToggleLock();

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bNewAiming);

	UFUNCTION(Server, Reliable)
	void ServerTryActivateAbility(TSubclassOf<UGameplayAbility> AbilityClass);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAttackMontage();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayShootMontage();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDodgeMontage();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayEquipMontage(FName SectionName);

	void ExecuteAttackAuthority();
	void ExecuteInteractAuthority();
	void ExecuteDodgeAuthority();
	void ExecuteToggleLockAuthority();
	void ExecuteSetAimingAuthority(bool bNewAiming);
	bool ActivateAbilityByClassInternal(TSubclassOf<UGameplayAbility> AbilityClass);

	bool bCanDisarm();
	bool bCanArm();
	void Disarm();
	void Arm();
	void EquipWeapon(AWeapon* Weapon);
	void EquipBow();

	void PlayEquipMontage(FName SectionName);
	void PlayShootMontage();
	virtual void Die() override;

	void LockOnToNearestEnemy(float DeltaTime);
	void FindNearestEnemy();

	UFUNCTION(BlueprintCallable)
	void FinishEquipping();

	UFUNCTION(BlueprintCallable)
	void AttachWeaponToBack();

	UFUNCTION(BlueprintCallable)
	void AttachWeaponToHand();

	UFUNCTION(BlueprintCallable)
	void HitReactEnd();

private:

	bool IsUnoccupied();
	float GetCurrentHealthPercentForHUD() const;
	float GetCurrentStaminaPercentForHUD() const;
	float GetCurrentStamina() const;
	float GetCurrentMaxStamina() const;
	void ConsumeStamina(float Cost);
	void RegenStaminaGAS(float DeltaTime);

	FTimerHandle HitReactionTimer;

	UPROPERTY(EditAnywhere)
	float OriginalAnimationSpeed = 1.f;

	UPROPERTY(EditAnywhere)
	float HitReactionDuration = 0.1f;

	UPROPERTY(EditAnywhere)
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere)
	float RunSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "GAS|Combat")
	float DodgeStaminaCost = 14.f;

	UPROPERTY(EditAnywhere, Category = "GAS|Combat")
	float StaminaRegenPerSecond = 8.f;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, meta = (AllowPrivateAccess = "true"))
	ECharacterState CharacterState = ECharacterState::ECS_Unequipped;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, meta = (AllowPrivateAccess = "true"))
	ECharacterMoveState MoveState = ECharacterMoveState::ECMS_Unlocked;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, meta = (AllowPrivateAccess = "true"))
	EActionState ActionState = EActionState::EAS_Unoccupied;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	ETurningInPlace TurningInPlace = ETurningInPlace::ETIP_NotTurning;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, meta = (AllowPrivateAccess = "true"))
	EBowState BowState = EBowState::EBS_NotUsingBow;

	UPROPERTY(VisibleAnywhere)
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere)
	UCameraComponent* ViewCamera;

	UPROPERTY(VisibleAnywhere, Category = Hair)
	UGroomComponent* Hair;

	UPROPERTY(VisibleAnywhere, Category = Hair)
	UGroomComponent* Eyebrows;

	UPROPERTY(VisibleInstanceOnly)
	AMyItem* OverlappingItem;

	UPROPERTY(EditDefaultsOnly, Category = Montages)
	UAnimMontage* EquipMontage;

	UPROPERTY(EditDefaultsOnly, Category = Montages)
	UAnimMontage* ShootMontage;

	UPROPERTY(EditDefaultsOnly, Category = Montages)
	UAnimMontage* VaultMontage;

	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* AttachedProjectile;

	UPROPERTY(VisibleAnywhere, Category ="Actor Attributes")
	USlashOverlap* SlashOverlay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	USlashAttributeSet* SlashAttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayAbility> DefaultAttackAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributesEffect;

	bool bStartupAbilitiesGranted = false;
	bool bStartupEffectsApplied = false;

	UPROPERTY()
	class ASlashHUD* HUD;

	class UTexture2D* CrosshairsCenter;
	
	void InitializeSlashOverlay();

	void SetHUDHealth();

	void SetHUDCrosshairs(float Deltatime);

	void TraceUnderCrosshairs(FHitResult& TraceHitResult);

	void HideCameraIfCharacterClose();

	int32 PlayAttackMontage() override;

	// Reference to the currently locked-on enemy
	AEnemy* LockedEnemy = nullptr;

	UPROPERTY(VisibleAnywhere)
	AEnemy* NearestEnemy = nullptr;

	float AO_Yaw;
	float InterpAO_Yaw;
	float AO_Pitch;
	FRotator StartingAimRotation;

	FVector HitTarget;

	/**
	* Aiming and FOV
	*/

	// Field of view when not aiming; set to the camera's base FOV in BeginPlay
	float DefaultFOV;

	UPROPERTY(EditAnywhere, Category = Camera)
	float ZoomedFOV = 30.f;

	float CurrentFOV;

	UPROPERTY(EditAnywhere, Category = Camera)
	float ZoomInterpSpeed = 20.f;

	UPROPERTY(EditAnywhere, Category = Camera)

	float CameraThreshold = 200.f;
	void InterpFOV(float DeltaTime);

	void TurnInPlace(float DeltaTime);
};

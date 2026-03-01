// Fill out your copyright notice in the Description page of Project Settings.


#include "SlashCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GroomComponent.h"
#include "MyItem.h"
#include "Weapons/Weapon.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "HUD/SlashHUD.h"
#include "HUD/SlashOverlap.h"
#include "AttributeComponents.h"
#include "Soul.h"
#include "Treasure.h"
#include <Kismet/GameplayStatics.h>
#include <Enemy/Enemy.h>
#include <Kismet/KismetMathLibrary.h>
#include "HUD/SlashHUD.h"
#include "Weapons/Projectile.h"
#include "SlashAnimInstance.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GAS/SlashAttributeSet.h"
#include "GAS/GE_SlashInitAttributes.h"
#include "GAS/GA_SlashAttack.h"
#include "SlashPlayerState.h"


// Sets default values
ASlashCharacter::ASlashCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 380.f, 0);

	PrimaryActorTick.bCanEverTick = true;
	GetMesh()->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	GetMesh()->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(true);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 365.f;
	CameraBoom->SocketOffset = FVector(0.0f, 100.f, 100.0f);

	//Camera Settings
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(CameraBoom);

	DefaultFOV = ViewCamera->FieldOfView;
	CurrentFOV = DefaultFOV;
	ViewCamera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
	ViewCamera->PostProcessSettings.DepthOfFieldFocalDistance = 10000;
	ViewCamera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
	ViewCamera->PostProcessSettings.DepthOfFieldFstop = 4.0;


	Hair = CreateDefaultSubobject<UGroomComponent>(TEXT("Hair"));
	Hair->SetupAttachment(GetMesh());
	Hair->AttachmentName = FString("head");

	Eyebrows = CreateDefaultSubobject<UGroomComponent>(TEXT("Eyebrows"));
	Eyebrows->SetupAttachment(GetMesh());
	Eyebrows->AttachmentName = FString("head");

	EquippedWeapon = nullptr;

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

	AttachedProjectile = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttachedProjectile"));
	AttachedProjectile->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	AttachedProjectile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttachedProjectile->SetupAttachment(GetRootComponent());
	AttachedProjectile->SetVisibility(false);

	AbilitySystemComponent = nullptr;
	SlashAttributeSet = nullptr;
	DefaultPrimaryAttributesEffect = UGE_SlashInitAttributes::StaticClass();
	DefaultAttackAbilityClass = UGA_SlashAttack::StaticClass();

}

void ASlashCharacter::GetHit_Implementation(const FVector& ImpactPoint, AActor* Hitter)
{
	Super::GetHit_Implementation(ImpactPoint, Hitter);
	SetWeaponCollisionEnabled(ECollisionEnabled::NoCollision);
	if (IsAlive())
	{
		ActionState = EActionState::EAS_HitReaction;
	}
}

void ASlashCharacter::SetOverlappingItem(AMyItem* Item)
{
	OverlappingItem = Item;
}

void ASlashCharacter::AddSouls(ASoul* Soul)
{
	if (!HasAuthority()) return;

	if (Attributes && SlashOverlay)
	{
		Attributes->AddSouls(Soul->GetSouls());
		SlashOverlay->SetSouls(Attributes->GetSouls());
	}
}

void ASlashCharacter::AddGold(ATreasure* Treasure)
{
	if (!HasAuthority()) return;

	if (Attributes && SlashOverlay)
	{
		Attributes->AddGold(Treasure->GetGold());
		SlashOverlay->SetGold(Attributes->GetGold());
	}
}

void ASlashCharacter::AimOffset(float DeltaTime)
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	float Speed = Velocity.Size();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir)
	{
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		const FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
	}
	if (Speed > 0.f || bIsInAir) //running jumping
	{
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	AO_Pitch = GetBaseAimRotation().Pitch;
}

void ASlashCharacter::StartHitReaction()
{
	// ͣ�ٶ����ĳ���ʱ�䣨�룩
	

	// ��ͣ����
	//APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	//if (PlayerController)
	//{
	//	PlayerController->SetIgnoreLookInput(true);
	//	PlayerController->SetIgnoreMoveInput(true);
	//}
	// 
	// �����ʱ���������У���ֹ��
	GetWorldTimerManager().ClearTimer(HitReactionTimer);

	// ���ü�ʱ����������һ��ʱ���ָ�����
	GetWorldTimerManager().SetTimer(HitReactionTimer, this, &ASlashCharacter::ResumeAction, HitReactionDuration, false);

	// ��ȡ����ʵ��
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		// ����ԭʼ���������ٶ�
		OriginalAnimationSpeed = AnimInstance->Montage_GetPlayRate(AttackMontage);

		// �������������ٶ�
		AnimInstance->Montage_SetPlayRate(AttackMontage, 0.05f);
	}
}

void ASlashCharacter::ResumeAction()
{
	// �ָ�ԭʼ���������ٶ�
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->Montage_SetPlayRate(AttackMontage, OriginalAnimationSpeed);
	}
}

UAbilitySystemComponent* ASlashCharacter::GetAbilitySystemComponent() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}

	if (const ASlashPlayerState* SlashPS = GetPlayerState<ASlashPlayerState>())
	{
		return SlashPS->GetAbilitySystemComponent();
	}

	return nullptr;
}

void ASlashCharacter::InitializeAbilitySystem()
{
	ASlashPlayerState* SlashPS = GetPlayerState<ASlashPlayerState>();
	if (!SlashPS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GAS][%s] SlashPlayerState missing. Set GameMode PlayerStateClass to SlashPlayerState."), *GetName());
		return;
	}

	AbilitySystemComponent = SlashPS->GetAbilitySystemComponent();
	SlashAttributeSet = SlashPS->GetSlashAttributeSet();

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GAS][%s] ASC missing on SlashPlayerState."), *GetName());
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(SlashPS, this);
}

void ASlashCharacter::GrantStartupAbilities()
{
	if (!HasAuthority()) return;
	if (!AbilitySystemComponent) return;
	if (bStartupAbilitiesGranted) return;

	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant = StartupAbilities;
	if (DefaultAttackAbilityClass)
	{
		AbilitiesToGrant.AddUnique(DefaultAttackAbilityClass);
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilitiesToGrant)
	{
		if (!AbilityClass) continue;

		bool bAlreadyGranted = false;
		for (const FGameplayAbilitySpec& ExistingSpec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (ExistingSpec.Ability && ExistingSpec.Ability->GetClass() == AbilityClass)
			{
				bAlreadyGranted = true;
				break;
			}
		}
		if (bAlreadyGranted)
		{
			continue;
		}

		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}

	bStartupAbilitiesGranted = true;
}

void ASlashCharacter::ApplyStartupEffects()
{
	if (!HasAuthority()) return;
	if (!AbilitySystemComponent) return;
	if (bStartupEffectsApplied) return;

	if (DefaultPrimaryAttributesEffect)
	{
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
			DefaultPrimaryAttributesEffect,
			1.f,
			EffectContext
		);
		if (SpecHandle.IsValid())
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	bStartupEffectsApplied = true;
	if (SlashAttributeSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GAS][%s] InitAttr Health=%.1f MaxHealth=%.1f Stamina=%.1f MaxStamina=%.1f AttackPower=%.1f"),
			*GetName(),
			SlashAttributeSet->GetHealth(),
			SlashAttributeSet->GetMaxHealth(),
			SlashAttributeSet->GetStamina(),
			SlashAttributeSet->GetMaxStamina(),
			SlashAttributeSet->GetAttackPower());
	}
}

bool ASlashCharacter::ActivateAbilityByClassInternal(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilitySystemComponent || !AbilityClass) return false;

	const TArray<FGameplayAbilitySpec>& Specs = AbilitySystemComponent->GetActivatableAbilities();
	for (const FGameplayAbilitySpec& Spec : Specs)
	{
		if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
		{
			return AbilitySystemComponent->TryActivateAbility(Spec.Handle);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[GAS][%s] Ability not granted: %s"),
		*GetName(), *GetNameSafe(AbilityClass));
	return false;
}

bool ASlashCharacter::TryActivateAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass || !AbilitySystemComponent) return false;

	bool bAbilityGranted = false;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
		{
			bAbilityGranted = true;
			break;
		}
	}

	if (!bAbilityGranted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GAS][%s] Ability spec missing for %s"),
			*GetName(), *GetNameSafe(AbilityClass));
		return false;
	}

	if (HasAuthority())
	{
		return ActivateAbilityByClassInternal(AbilityClass);
	}

	ServerTryActivateAbility(AbilityClass);
	return true;
}

void ASlashCharacter::ServerTryActivateAbility_Implementation(TSubclassOf<UGameplayAbility> AbilityClass)
{
	ActivateAbilityByClassInternal(AbilityClass);
}

void ASlashCharacter::TriggerAttackFromAbility()
{
	if (HasAuthority())
	{
		ExecuteAttackAuthority();
	}
	else
	{
		ServerRequestAttack();
	}
}

// Called when the game starts or when spawned
void ASlashCharacter::BeginPlay()
{


	Super::BeginPlay();
	const FString NetModeStr = ToString(GetNetMode()); // ENetMode 用 ToString
	const FString RoleStr = UEnum::GetValueAsString(TEXT("Engine.ENetRole"), GetLocalRole());

	UE_LOG(LogTemp, Warning, TEXT("[网络调试][PID=%d][世界=%s][角色=%s] 模式=%s 角色权限=%s Auth=%d 本地控制=%d 控制器=%s"),
	FPlatformProcess::GetCurrentProcessId(),
	*GetWorld()->GetName(),
	*GetName(),
	*ToString(GetNetMode()),
	*UEnum::GetValueAsString(TEXT("Engine.ENetRole"), GetLocalRole()),
	HasAuthority() ? 1 : 0,
	IsLocallyControlled() ? 1 : 0,
	*GetNameSafe(GetController()));
	InitializeSlashOverlay();
	Tags.Add(FName("EngageableTarget"));
	FAttachmentTransformRules TransformRules(EAttachmentRule::SnapToTarget, true);
	AttachedProjectile->AttachToComponent(GetMesh(), TransformRules, FName("RightIndexSocket"));
	FindNearestEnemy();
	InitializeAbilitySystem();
	if (HasAuthority())
	{
		GrantStartupAbilities();
		ApplyStartupEffects();
	}

}

void ASlashCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASlashCharacter, CharacterState);
	DOREPLIFETIME(ASlashCharacter, MoveState);
	DOREPLIFETIME(ASlashCharacter, ActionState);
	DOREPLIFETIME(ASlashCharacter, BowState);
}

void ASlashCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
	GrantStartupAbilities();
	ApplyStartupEffects();
	UE_LOG(LogTemp, Warning, TEXT("[控制链][PossessedBy][PID=%d][%s] 控制器=%s"),
		FPlatformProcess::GetCurrentProcessId(),
		*GetName(),
		*GetNameSafe(NewController));
	InitializeSlashOverlay();
}

void ASlashCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitializeAbilitySystem();
	UE_LOG(LogTemp, Warning, TEXT("[控制链][OnRep_Controller][PID=%d][%s] 控制器=%s"),
		FPlatformProcess::GetCurrentProcessId(),
		*GetName(),
		*GetNameSafe(GetController()));
	InitializeSlashOverlay();
}

void ASlashCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
	InitializeSlashOverlay();
}

void ASlashCharacter::InitializeSlashOverlay()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
		if (Subsystem) {
			Subsystem->AddMappingContext(SlashContext, 0);
		}
		ASlashHUD* SlashHUD = Cast<ASlashHUD>(PlayerController->GetHUD());
		if (SlashHUD)
		{
			SlashOverlay = SlashHUD->GetSlashOverlay();
			if (SlashOverlay)
			{
				SlashOverlay->SetHealthBarPercent(GetCurrentHealthPercentForHUD());
				SlashOverlay->SetStaminaPercent(GetCurrentStaminaPercentForHUD());
				SlashOverlay->SetGold(Attributes ? Attributes->GetGold() : 0);
				SlashOverlay->SetSouls(Attributes ? Attributes->GetSouls() : 0);
			}
		}
	}
}

void ASlashCharacter::Move(const FInputActionValue& Value)
{
	if (ActionState != EActionState::EAS_Unoccupied)return;
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller) {

		const FRotator ControlRotation = GetControlRotation();
		const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);

		const FVector DirectionForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);//forward

		//FVector Forward = GetActorForwardVector();
		AddMovementInput(DirectionForward, MovementVector.Y);

		const FVector DirectionRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);//right and left
		//FVector Right = GetActorRightVector();
		AddMovementInput(DirectionRight, MovementVector.X);
		
	}
}

void ASlashCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisValue = Value.Get<FVector2D>();
	if (Controller) {
		AddControllerYawInput(LookAxisValue.X);
		AddControllerPitchInput(LookAxisValue.Y);
	}
}

void ASlashCharacter::EKeyPressed(const FInputActionValue& Value)
{
	if (HasAuthority())
	{
		ExecuteInteractAuthority();
	}
	else
	{
		ServerInteract();
	}
}

void ASlashCharacter::Dodge(const FInputActionValue& Value)
{
	if (HasAuthority())
	{
		ExecuteDodgeAuthority();
	}
	else
	{
		ServerRequestDodge();
	}
}

void ASlashCharacter::ChangeToLockedControl()
{
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void ASlashCharacter::ChangeToUnlockedControl()
{
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
}

void ASlashCharacter::Lock(const FInputActionValue& Value)
{
	if (HasAuthority())
	{
		ExecuteToggleLockAuthority();
	}
	else
	{
		ServerToggleLock();
	}
}

void ASlashCharacter::AimButtonPressed(const FInputActionValue& Value)
{
	if (HasAuthority())
	{
		ExecuteSetAimingAuthority(true);
	}
	else
	{
		ServerSetAiming(true);
	}
}

void ASlashCharacter::DrawArrow()
{
	BowState = EBowState::EBS_DrawingArrow;
	PlayMontageSection(ShootMontage, FName("DrawArrow"));
	AttachArrow();
}

void ASlashCharacter::AimButtonReleased(const FInputActionValue& Value)
{
	if (HasAuthority())
	{
		ExecuteSetAimingAuthority(false);
	}
	else
	{
		ServerSetAiming(false);
	}

	AttachedProjectile->SetVisibility(false);
}

void ASlashCharacter::Attack(const FInputActionValue& Value)
{
	Super::Attack(Value);
	// if (CanAttack())
	// {
	// 	FindNearestEnemy();
	// 	ActionState = EActionState::EAS_Attacking;
	// 	PlayAttackMontage();
	// }
	// else if (CanShoot())
	// {
	// 	PlayShootMontage();
	// 	BowState = EBowState::EBS_Shooting;
	// 	EquippedWeapon->Fire(HitTarget, GetMesh(), FName("RightIndexSocket"));
	// 	if (AttachedProjectile)AttachedProjectile->SetVisibility(false);
	// }
	if (DefaultAttackAbilityClass && TryActivateAbilityByClass(DefaultAttackAbilityClass))
	{
		return;
	}

	if (HasAuthority())
	{
		ExecuteAttackAuthority();
	}
	else
	{
		ServerRequestAttack();
	}
}
void ASlashCharacter::ServerRequestAttack_Implementation()
{
	ExecuteAttackAuthority();
}

void ASlashCharacter::ServerRequestDodge_Implementation()
{
	ExecuteDodgeAuthority();
}

void ASlashCharacter::ServerToggleLock_Implementation()
{
	ExecuteToggleLockAuthority();
}

void ASlashCharacter::ServerSetAiming_Implementation(bool bNewAiming)
{
	ExecuteSetAimingAuthority(bNewAiming);
}

void ASlashCharacter::ServerInteract_Implementation()
{
	ExecuteInteractAuthority();
}

void ASlashCharacter::ExecuteAttackAuthority()
{
	if (CanAttack())
	{
		FindNearestEnemy();
		ActionState = EActionState::EAS_Attacking;
		MulticastPlayAttackMontage();
	}
	else if (CanShoot())
	{
		MulticastPlayShootMontage();
		BowState = EBowState::EBS_Shooting;
		EquippedWeapon->Fire(HitTarget, GetMesh(), FName("RightIndexSocket"));
		if (AttachedProjectile) AttachedProjectile->SetVisibility(false);
	}
}

void ASlashCharacter::ExecuteDodgeAuthority()
{
	if (ActionState != EActionState::EAS_Unoccupied) return;
	if (GetCurrentStamina() < DodgeStaminaCost) return;

	ActionState = EActionState::EAS_Dodge;
	const FVector LastInput = GetCharacterMovement()->GetLastInputVector();
	if (!LastInput.IsNearlyZero())
	{
		const FRotator NewRotation = UKismetMathLibrary::Conv_VectorToRotator(LastInput);
		SetActorRotation(NewRotation);
	}
	ConsumeStamina(DodgeStaminaCost);
	MulticastPlayDodgeMontage();
}

void ASlashCharacter::ExecuteToggleLockAuthority()
{
	if (MoveState == ECharacterMoveState::ECMS_Unlocked)
	{
		MoveState = ECharacterMoveState::ECMS_Locked;
		ChangeToLockedControl();
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			PlayerController->SetIgnoreLookInput(true);
		}
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		FindNearestEnemy();
	}
	else if (MoveState == ECharacterMoveState::ECMS_Locked)
	{
		MoveState = ECharacterMoveState::ECMS_Unlocked;
		ChangeToUnlockedControl();
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			PlayerController->SetIgnoreLookInput(false);
		}
		if (LockedEnemy)
		{
			LockedEnemy->CancelSelect();
		}
		NearestEnemy = nullptr;
		LockedEnemy = nullptr;
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	}
}

void ASlashCharacter::ExecuteSetAimingAuthority(bool bNewAiming)
{
	if (CharacterState != ECharacterState::ECS_EquippedBow) return;

	if (bNewAiming)
	{
		MoveState = ECharacterMoveState::ECMS_Aiming;
		BowState = EBowState::EBS_Aiming;
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
	else
	{
		MoveState = ECharacterMoveState::ECMS_Unlocked;
		ActionState = EActionState::EAS_Unoccupied;
		BowState = EBowState::EBS_NotUsingBow;
		ChangeToUnlockedControl();
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	}
}

void ASlashCharacter::MulticastPlayAttackMontage_Implementation()
{
	if (ActionState != EActionState::EAS_Dead)
	{
		PlayAttackMontage();
	}
}

void ASlashCharacter::MulticastPlayShootMontage_Implementation()
{
	if (ActionState != EActionState::EAS_Dead)
	{
		PlayShootMontage();
	}
}

void ASlashCharacter::MulticastPlayDodgeMontage_Implementation()
{
	if (ActionState != EActionState::EAS_Dead)
	{
		PlayDodgeMontage();
	}
}

void ASlashCharacter::MulticastPlayEquipMontage_Implementation(FName SectionName)
{
	if (ActionState != EActionState::EAS_Dead)
	{
		PlayEquipMontage(SectionName);
	}
}

void ASlashCharacter::ExecuteInteractAuthority()
{
	AWeapon* OverlappingWeapon = Cast<AWeapon>(OverlappingItem);
	if (OverlappingWeapon)
	{
		EquipWeapon(OverlappingWeapon);
	}
	else
	{
		if (bCanDisarm())
		{
			Disarm();
		}
		else if (bCanArm())
		{
			Arm();
		}
	}
}
void ASlashCharacter::WallRun(const FInputActionValue& Value)
{
}

void ASlashCharacter::EquipWeapon(AWeapon* Weapon)
{
	switch (Weapon->WeaponType)
	{
	case(EWeaponType::EWT_ShortSword):
	case(EWeaponType::EWT_LongSword):
	{
		Weapon->Equip(GetMesh(), FName("RightHandSocket"), this, this);
		CharacterState = ECharacterState::ECS_EquippedOneHandedWeapon;
	}break;
	case(EWeaponType::EWT_Bow):
	{
		Weapon->Equip(GetMesh(), FName("LeftHandSocket"), this, this);
		CharacterState = ECharacterState::ECS_EquippedBow;
	}
	default:
		break;
	}
	OverlappingItem = nullptr;
	EquippedWeapon = Weapon;
}

void ASlashCharacter::EquipBow()
{
	CharacterState = ECharacterState::ECS_EquippedBow;
}

bool ASlashCharacter::CanAttack()
{
	return (ActionState == EActionState::EAS_Unoccupied || ActionState == EActionState::EAS_AttackFinish) &&
		CharacterState == ECharacterState::ECS_EquippedOneHandedWeapon;
}

bool ASlashCharacter::CanShoot()
{
	return
		MoveState == ECharacterMoveState::ECMS_Aiming &&
		BowState == EBowState::EBS_Ready &&
		CharacterState == ECharacterState::ECS_EquippedBow;
}

void ASlashCharacter::AttackFinish()
{
	if (!HasAuthority()) return;
	ActionState = EActionState::EAS_AttackFinish;
}

bool ASlashCharacter::bCanDisarm()
{
	return ActionState == EActionState::EAS_Unoccupied &&
		CharacterState != ECharacterState::ECS_Unequipped;
}

bool ASlashCharacter::bCanArm()
{
	return ActionState == EActionState::EAS_Unoccupied &&
		CharacterState == ECharacterState::ECS_Unequipped &&
		EquippedWeapon;
}

void ASlashCharacter::PlayEquipMontage(FName SectionName)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && EquipMontage) {
		AnimInstance->Montage_Play(EquipMontage);
		AnimInstance->Montage_JumpToSection(SectionName, EquipMontage);
	}
}

void ASlashCharacter::PlayShootMontage()
{
	PlayMontageSection(ShootMontage, FName("Shoot"));
}

void ASlashCharacter::Die()
{
	Super::Die();

	ActionState = EActionState::EAS_Dead;
}

void ASlashCharacter::LockOnToNearestEnemy(float DeltaTime)
{
	// Find the nearest enemy
	if (LockedEnemy == nullptr)return;

	// Adjust camera and character rotation to focus on the locked enemy
	if (LockedEnemy)
	{
		// Calculate the look at rotation
		FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), LockedEnemy->GetActorLocation());

		// Interpolate the rotation smoothly
		FRotator SmoothRotation = FMath::RInterpTo(GetControlRotation(), LookAtRotation, DeltaTime, 100.0f);

		// Set the new rotation
		//SetActorRotation(SmoothRotation);
		GetController()->SetControlRotation(SmoothRotation);
		// (Optionally) Set camera rotation if using a SpringArmComponent
		if (CameraBoom)
		{
			CameraBoom->SetRelativeRotation(SmoothRotation);
		}
		
	}
}

void ASlashCharacter::FindNearestEnemy()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemy::StaticClass(), FoundActors);

	float NearestDistanceSquared = MAX_FLT;

	AActor* NearestActor = nullptr;

	for (AActor* Enemy : FoundActors)
	{
		float Distance = FVector::DistSquared(GetActorLocation(), Enemy->GetActorLocation());
		if ((Distance < NearestDistanceSquared) && !(Enemy->ActorHasTag(FName("Dead"))))
		{
			NearestActor = Enemy;
			NearestDistanceSquared = Distance;
		}
	}
	if (NearestActor != nullptr && NearestDistanceSquared < WarpDistanceSquared)
	{
		NearestEnemy = Cast<AEnemy>(NearestActor);
		UE_LOG(LogTemp, Warning, TEXT("SquaredDistance: %f"), NearestDistanceSquared);
		LockedEnemy = NearestEnemy;
		CombatTarget = NearestEnemy;
		//LockedEnemy->Select();
		
	}
	else
	{
		CombatTarget = nullptr;
	}
}

void ASlashCharacter::AttackEnd()
{
	if (!HasAuthority()) return;
	ActionState = EActionState::EAS_Unoccupied;
}

void ASlashCharacter::DodgeEnd()
{
	if (!HasAuthority()) return;
	Super::DodgeEnd();
	ActionState = EActionState::EAS_Unoccupied;
}

void ASlashCharacter::ShootEnd()
{
	if (!HasAuthority()) return;
	BowState = EBowState::EBS_Aiming;
}

void ASlashCharacter::AttachArrow()
{
	AttachedProjectile->SetVisibility(true);
}

void ASlashCharacter::DrawEnd()
{
	if (!HasAuthority()) return;
	BowState = EBowState::EBS_Ready;
}

void ASlashCharacter::FinishEquipping()
{
	if (!HasAuthority()) return;
	ActionState = EActionState::EAS_Unoccupied;
}
void ASlashCharacter::AttachWeaponToBack()
{
	if (EquippedWeapon)
	{
		if (EquippedWeapon->WeaponType == EWeaponType::EWT_ShortSword)
			EquippedWeapon->AttachMeshToSocket(GetMesh(), FName("SpineSocket"));
		if (EquippedWeapon->WeaponType == EWeaponType::EWT_Bow)
			EquippedWeapon->AttachMeshToSocket(GetMesh(), FName("SpineBowSocket"));
	}
}

void ASlashCharacter::AttachWeaponToHand()
{
	if (EquippedWeapon)
	{
		if(EquippedWeapon->WeaponType==EWeaponType::EWT_ShortSword)
			EquippedWeapon->AttachMeshToSocket(GetMesh(), FName("RightHandSocket"));
		if(EquippedWeapon->WeaponType == EWeaponType::EWT_Bow)
			EquippedWeapon->AttachMeshToSocket(GetMesh(), FName("LeftHandSocket"));
	}
}
void ASlashCharacter::HitReactEnd()
{
	if (!HasAuthority()) return;
	ActionState = EActionState::EAS_Unoccupied;
}
void ASlashCharacter::Disarm()
{
	if (!EquippedWeapon) return;
	if (EquippedWeapon->WeaponType == EWeaponType::EWT_ShortSword || EquippedWeapon->WeaponType == EWeaponType::EWT_LongSword)
	{
		MulticastPlayEquipMontage(FName("Unequip"));
	}
	else if (EquippedWeapon->WeaponType == EWeaponType::EWT_Bow)
	{
		MulticastPlayEquipMontage(FName("UnequipBow"));
	}
	CharacterState = ECharacterState::ECS_Unequipped;
	ActionState = EActionState::EAS_EquippingWeapon;

}

void ASlashCharacter::Arm()
{
	if (!EquippedWeapon) return;
	if (EquippedWeapon->WeaponType == EWeaponType::EWT_ShortSword || EquippedWeapon->WeaponType == EWeaponType::EWT_LongSword)
	{
		MulticastPlayEquipMontage(FName("Equip"));
		CharacterState = ECharacterState::ECS_EquippedOneHandedWeapon;
	}
	else if (EquippedWeapon->WeaponType == EWeaponType::EWT_Bow)
	{
		MulticastPlayEquipMontage(FName("EquipBow"));
		CharacterState = ECharacterState::ECS_EquippedBow;
	}
	ActionState = EActionState::EAS_EquippingWeapon;
}

// Called every frame
void ASlashCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (HasAuthority())
	{
		RegenStaminaGAS(DeltaTime);
	}

	if (SlashOverlay)
	{
		SlashOverlay->SetHealthBarPercent(GetCurrentHealthPercentForHUD());
		SlashOverlay->SetStaminaPercent(GetCurrentStaminaPercentForHUD());
		SlashOverlay->SetGold(Attributes ? Attributes->GetGold() : 0);
		SlashOverlay->SetSouls(Attributes ? Attributes->GetSouls() : 0);
	}
	
	if (MoveState == ECharacterMoveState::ECMS_Locked)
	{
		LockOnToNearestEnemy(DeltaTime);
	}

	else if (MoveState == ECharacterMoveState::ECMS_Aiming)
	{
		AimOffset(DeltaTime);
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		if(BowState == EBowState::EBS_Aiming)DrawArrow();
		//SetHUDCrosshairs(DeltaTime);
	}
	InterpFOV(DeltaTime);
}

// Called to bind functionality to input
void ASlashCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MovementAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Look);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Jump);
		// Interact should fire once per key press, otherwise Triggered can spam ServerInteract every frame.
		EnhancedInputComponent->BindAction(EKeyAction, ETriggerEvent::Started, this, &ASlashCharacter::EKeyPressed);
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &ASlashCharacter::Dodge);
		// Attack should fire once per click to avoid spamming server RPC every frame.
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ASlashCharacter::Attack);
		EnhancedInputComponent->BindAction(LockAction, ETriggerEvent::Started, this, &ASlashCharacter::Lock);
		EnhancedInputComponent->BindAction(AimStartAction, ETriggerEvent::Started, this, &ASlashCharacter::AimButtonPressed);
		EnhancedInputComponent->BindAction(AimEndAction, ETriggerEvent::Started, this, &ASlashCharacter::AimButtonReleased);
		EnhancedInputComponent->BindAction(WallRunAction, ETriggerEvent::Triggered, this, &ASlashCharacter::WallRun);
	}

}

void ASlashCharacter::Jump()
{
	if (IsUnoccupied())
	{
		Super::Jump();

	}
}

bool ASlashCharacter::IsUnoccupied()
{
	return ActionState == EActionState::EAS_Unoccupied;
}

float ASlashCharacter::GetCurrentHealthPercentForHUD() const
{
	if (SlashAttributeSet)
	{
		const float MaxHealth = FMath::Max(SlashAttributeSet->GetMaxHealth(), 1.f);
		return SlashAttributeSet->GetHealth() / MaxHealth;
	}
	return Attributes ? Attributes->GetHealthPercent() : 0.f;
}

float ASlashCharacter::GetCurrentStaminaPercentForHUD() const
{
	const float MaxStamina = GetCurrentMaxStamina();
	if (MaxStamina <= KINDA_SMALL_NUMBER) return 0.f;
	return GetCurrentStamina() / MaxStamina;
}

float ASlashCharacter::GetCurrentStamina() const
{
	if (SlashAttributeSet)
	{
		return SlashAttributeSet->GetStamina();
	}
	return Attributes ? static_cast<float>(Attributes->GetStamina()) : 0.f;
}

float ASlashCharacter::GetCurrentMaxStamina() const
{
	if (SlashAttributeSet)
	{
		return SlashAttributeSet->GetMaxStamina();
	}
	return 100.f;
}

void ASlashCharacter::ConsumeStamina(float Cost)
{
	if (Cost <= 0.f) return;

	if (SlashAttributeSet)
	{
		const float NewStamina = FMath::Clamp(SlashAttributeSet->GetStamina() - Cost, 0.f, SlashAttributeSet->GetMaxStamina());
		SlashAttributeSet->SetStamina(NewStamina);
		return;
	}

	if (Attributes)
	{
		Attributes->UseStamina(Cost);
	}
}

void ASlashCharacter::RegenStaminaGAS(float DeltaTime)
{
	if (DeltaTime <= 0.f) return;

	if (SlashAttributeSet)
	{
		const float NewStamina = FMath::Clamp(
			SlashAttributeSet->GetStamina() + StaminaRegenPerSecond * DeltaTime,
			0.f,
			SlashAttributeSet->GetMaxStamina()
		);
		SlashAttributeSet->SetStamina(NewStamina);
		return;
	}

	if (Attributes)
	{
		Attributes->RegenStamina(DeltaTime);
	}
}

float ASlashCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	HandleDamage(DamageAmount, DamageCauser);
	SetHUDHealth();
	return DamageAmount;
}

void ASlashCharacter::SetHUDHealth()
{
	if (SlashOverlay)
	{
		SlashOverlay->SetHealthBarPercent(GetCurrentHealthPercentForHUD());
	}
}

//void ASlashCharacter::SetHUDCrosshairs(float Deltatime)
//{
//	HUD = HUD == nullptr ? Cast<ASlashHUD>(Controller->GetHUD()) : HUD;
//	if (HUD)
//	{
//		HUD->DrawHUD();
//	}
//}

void ASlashCharacter::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	FVector2D viewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(viewportSize);
	}

	FVector2D CrosshairLocation(viewportSize.X / 2.f, viewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld
	(UGameplayStatics::GetPlayerController(this, 0), CrosshairLocation, 
		CrosshairWorldPosition, CrosshairWorldDirection);
	if (bScreenToWorld)
	{
		FVector Start = CrosshairWorldPosition;

		float DistanceToCharacter = (GetActorLocation() - Start).Size();
		Start += CrosshairWorldDirection * (DistanceToCharacter + 100.f);

		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;

		GetWorld()->LineTraceSingleByChannel(TraceHitResult, Start, End, ECollisionChannel::ECC_Visibility);
		if (!TraceHitResult.bBlockingHit)
		{
			TraceHitResult.ImpactPoint = End;
			HitTarget = End;
			DrawDebugSphere(GetWorld(), TraceHitResult.ImpactPoint, 12.f, 6, FColor::Green);
		}
		else
		{
			HitTarget = TraceHitResult.ImpactPoint;
			if (TraceHitResult.GetActor()->ActorHasTag(TEXT("Enemy")))
			{
				DrawDebugSphere(GetWorld(), TraceHitResult.ImpactPoint, 12.f, 6, FColor::Red);
			}
			else
			{
				DrawDebugSphere(GetWorld(), TraceHitResult.ImpactPoint, 12.f, 6, FColor::Green);
			}
		}
		
	}
}

void ASlashCharacter::HideCameraIfCharacterClose()
{
	if ((ViewCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		GetMesh()->SetVisibility(false);
		if (EquippedWeapon && EquippedWeapon->GetMesh())
		{
			EquippedWeapon->GetMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(false);
		if (EquippedWeapon && EquippedWeapon->GetMesh())
		{
			EquippedWeapon->GetMesh()->bOwnerNoSee = false;
		}
	}
}

int32 ASlashCharacter::PlayAttackMontage()
{
	return PlayNextMontageSection(AttackMontage, AttackMontageSections);
}

void ASlashCharacter::InterpFOV(float DeltaTime)
{
	if (EquippedWeapon == nullptr) return;

	if (MoveState == ECharacterMoveState::ECMS_Aiming)
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
	}
	else
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}
	if(ViewCamera)
		ViewCamera->SetFieldOfView(CurrentFOV);
	
}

void ASlashCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -70.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 5.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

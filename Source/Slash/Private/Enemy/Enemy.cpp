// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Enemy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Slash/DebugMacros.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ActorComponent.h"
#include "HUD/HealthBarComponent.h"
#include "Perception/PawnSensingComponent.h"
#include "AttributeComponents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "AI/EnemyAIController.h"
#include "Soul.h"
#include "Weapons/Weapon.h"
#include "Slash/Slash.h"

// Fill out your copyright notice in the Description page of Project Settings.

AEnemy::AEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECollisionResponse::ECR_Block);
	GetMesh()->SetGenerateOverlapEvents(true);


	HealthBarWidget = CreateDefaultSubobject<UHealthBarComponent>(TEXT("HealthBar"));
	HealthBarWidget->SetupAttachment(GetRootComponent());

	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	PawnSensing = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensing"));
	PawnSensing->SightRadius = 4000.f;
	PawnSensing->SetPeripheralVisionAngle(45.f);

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	EnemyState = EEnemyState::EES_NoState;
}

void AEnemy::SetEnemyState(EEnemyState NewState, const TCHAR* Reason)
{
	if (EnemyState == NewState) return;

	if (bEnableAIStateLog)
	{
		const UEnum* EnemyStateEnum = StaticEnum<EEnemyState>();
		const FString OldState = EnemyStateEnum ? EnemyStateEnum->GetNameStringByValue(static_cast<int64>(EnemyState)) : TEXT("Unknown");
		const FString NextState = EnemyStateEnum ? EnemyStateEnum->GetNameStringByValue(static_cast<int64>(NewState)) : TEXT("Unknown");
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AI鐘舵€乚[%s] %s -> %s 鍘熷洜=%s NetMode=%s"),
			*GetName(),
			*OldState,
			*NextState,
			Reason ? Reason : TEXT("Unknown"),
			*ToString(GetNetMode())
		);
	}

	EnemyState = NewState;
}

void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsDead()) return;
	if (IsUsingBehaviorTree()) return;

	if (EnemyState > EEnemyState::EES_Patrolling)
	{
		CheckCombatTarget();
	}
	else
	{
		CheckPatrolTarget();
	}
}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	HandleDamage(DamageAmount);

	if (EventInstigator)
	{
		CombatTarget = EventInstigator->GetPawn();
	}

	if (IsUsingBehaviorTree())
	{
		ShowHealthBar();
		if (CombatTarget && !IsDead())
		{
			EnterChasingStateFromBT();
		}
		return DamageAmount;
	}

	if (IsInsideAttackRadius())
	{
		EnemyState = EEnemyState::EES_Attacking;
	}
	else if (IsOutsideAttackRadius())
	{
		ChaseTarget();
	}
	return DamageAmount;
}

void AEnemy::Destroyed()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
	}
}

void AEnemy::GetHit_Implementation(const FVector& ImpactPoint, AActor* Hitter)
{
	if (IsDead()) return;

	UE_LOG(LogTemp, Warning, TEXT("enemy's gethit being called"));
	Super::GetHit_Implementation(ImpactPoint, Hitter);
	if (IsDead()) return;
	if(!IsDead())ShowHealthBar();
	ClearPatrolTimer();
	ClearAttackTimer();
	SetWeaponCollisionEnabled(ECollisionEnabled::NoCollision);
	StopAttackMontage();

	if (IsUsingBehaviorTree()) return;

	if (IsInsideAttackRadius() && IsAlive())
	{
		StartAttackTimer();
	}

}

UWorldUserWidget* AEnemy::GetTargetWidget() const
{
	return ActiveTargetWidget;
}

void AEnemy::Select()
{
	if (ActiveTargetWidget == nullptr)
	{
		ActiveTargetWidget = CreateWidget<UWorldUserWidget>(GetWorld(), TargetBarWidgetClass);
		if (ActiveTargetWidget)
		{
			ActiveTargetWidget->AttachedActor = this;
			ActiveTargetWidget->AddToViewport();
		}
	}
	else
	{
		ActiveTargetWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void AEnemy::CancelSelect()
{
	if (ActiveTargetWidget)
	{
		ActiveTargetWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	const FString NetModeStr = ToString(GetNetMode()); // ENetMode 鐢?ToString
	const FString RoleStr = UEnum::GetValueAsString(TEXT("Engine.ENetRole"), GetLocalRole());

	UE_LOG(LogTemp, Warning, TEXT("[缃戠粶璋冭瘯][鏁屼汉=%s] 妯″紡=%s 瑙掕壊鏉冮檺=%s Auth=%d"),
		*GetName(), *NetModeStr, *RoleStr, HasAuthority() ? 1 : 0);


	if (PawnSensing) PawnSensing->OnSeePawn.AddDynamic(this, &AEnemy::PawnSeen);
	InitializeEnemy();
	Tags.Add(FName("Enemy"));
}

void AEnemy::SpawnSoul()
{
	UWorld* World = GetWorld();
	if (World && SoulClass && Attributes)
	{
		FVector SpawnLocation = GetActorLocation() + FVector(0.f, 0.f, 0.f);
		ASoul* SpawnedSoul = World->SpawnActor<ASoul>(SoulClass, SpawnLocation, GetActorRotation());
		if(SpawnedSoul)SpawnedSoul->SetSouls(Attributes->GetSouls());
	}
}

void AEnemy::Die()
{
	if (IsDead()) return;

	Super::Die();
	EnemyState = EEnemyState::EES_Dead;
	ClearAttackTimer();
	ClearPatrolTimer();
	HideHealthBar();
	SetLifeSpan(DeathLifeSpan);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->DisableMovement();
	SpawnSoul();

	if (EnemyController)
	{
		EnemyController->StopMovement();
		if (UBrainComponent* BrainComponent = EnemyController->GetBrainComponent())
		{
			BrainComponent->StopLogic(TEXT("EnemyDead"));
		}
	}
}

void AEnemy::Attack()
{
	Super::Attack();
	if (CombatTarget == nullptr || IsDead()) return;
	if (EnemyController)
	{
		EnemyController->StopMovement();
	}

	const int32 AttackSection = PlayAttackMontage();
	if (AttackSection >= 0)
	{
		EnemyState = EEnemyState::EES_Engaged;
	}
	else
	{
		// No valid attack montage section: keep AI from getting stuck in engaged state.
		EnemyState = EEnemyState::EES_NoState;
	}
}

bool AEnemy::CanAttack()
{
	bool bCanAttack =
		IsInsideAttackRadius() &&
		!IsAttacking() &&
		!IsEngaged() &&
		!IsDead();
	return bCanAttack;
}

void AEnemy::AttackEnd()
{
	EnemyState = EEnemyState::EES_NoState;
	if (IsUsingBehaviorTree()) return;
	CheckCombatTarget();
}

void AEnemy::HandleDamage(float DamageAmount)
{
	Super::HandleDamage(DamageAmount);

	if (Attributes && HealthBarWidget)
	{
		HealthBarWidget->SetHealthPercent(Attributes->GetHealthPercent());
	}
}


void AEnemy::InitializeEnemy()
{
	EnemyController = Cast<AAIController>(GetController());
	HideHealthBar();
	SpawnDefaultWeapon();

	if (IsUsingBehaviorTree())
	{
		if (AEnemyAIController* BTController = Cast<AEnemyAIController>(EnemyController))
		{
			BTController->StartBehaviorTree(BehaviorTreeAsset, this);
			return;
		}
	}

	GetCharacterMovement()->MaxWalkSpeed = PatrollingSpeed;
	MoveToTarget(PatrolTarget);
}

void AEnemy::CheckPatrolTarget()
{
	if (InTargetRange(PatrolTarget, PatrolRadius))
	{
		PatrolTarget = ChoosePatrolTarget();
		const float WaitTime = FMath::RandRange(PatrolWaitMin, PatrolWaitMax);
		GetWorldTimerManager().SetTimer(PatrolTimer, this, &AEnemy::PatrolTimerFinished, WaitTime);
	}
}

void AEnemy::CheckCombatTarget()
{
	if (IsOutsideCombatRadius())
	{
		ClearAttackTimer();
		LoseInterest();
		if (!IsEngaged()) StartPatrolling();
	}
	else if (IsOutsideAttackRadius() && !IsChasing())
	{
		ClearAttackTimer();
		if (!IsEngaged()) ChaseTarget();
	}
	else if (CanAttack())
	{
		StartAttackTimer();
	}
}

void AEnemy::PatrolTimerFinished()
{
	MoveToTarget(PatrolTarget);
}

void AEnemy::HideHealthBar()
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(false);
	}
}

void AEnemy::ShowHealthBar()
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(true);
	}
}

void AEnemy::LoseInterest()
{
	CombatTarget = nullptr;
	HideHealthBar();
}

void AEnemy::StartPatrolling()
{
	EnemyState = EEnemyState::EES_Patrolling;
	GetCharacterMovement()->MaxWalkSpeed = PatrollingSpeed;
	MoveToTarget(PatrolTarget);
}

void AEnemy::ChaseTarget()
{
	EnemyState = EEnemyState::EES_Chasing;
	GetCharacterMovement()->MaxWalkSpeed = ChasingSpeed;
	MoveToTarget(CombatTarget);
}

bool AEnemy::IsOutsideCombatRadius()
{
	return(!InTargetRange(CombatTarget, CombatRadius));
}

bool AEnemy::IsOutsideAttackRadius()
{
	return !InTargetRange(CombatTarget, AttackRadius);
}

bool AEnemy::IsInsideAttackRadius()
{
	return InTargetRange(CombatTarget, AttackRadius);
}

bool AEnemy::IsChasing()
{
	return EnemyState == EEnemyState::EES_Chasing;
}

bool AEnemy::IsAttacking()
{
	return EnemyState == EEnemyState::EES_Attacking;
}

bool AEnemy::IsDead()
{
	return EnemyState == EEnemyState::EES_Dead;
}

bool AEnemy::IsEngaged()
{
	return EnemyState == EEnemyState::EES_Engaged;
}

void AEnemy::ClearPatrolTimer()
{
	GetWorldTimerManager().ClearTimer(PatrolTimer);
}

void AEnemy::StartAttackTimer()
{
	EnemyState = EEnemyState::EES_Attacking;
	const float AttackTime = FMath::RandRange(AttackMin, AttackMax);
	GetWorldTimerManager().SetTimer(AttackTimer, this, &AEnemy::Attack, AttackTime);
}

void AEnemy::ClearAttackTimer()
{
	GetWorldTimerManager().ClearTimer(AttackTimer);
}

bool AEnemy::InTargetRange(AActor* Target, double Radius)
{
	if (Target == nullptr) return false;
	const double DistanceToTarget = (Target->GetActorLocation() - GetActorLocation()).Size();
	return DistanceToTarget <= Radius;
}

void AEnemy::MoveToTarget(AActor* Target)
{
	if (EnemyController == nullptr || Target == nullptr) return;
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(Target);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	EnemyController->MoveTo(MoveRequest);
}

AActor* AEnemy::ChoosePatrolTarget()
{
	if (PatrolTargets.Num() <= 0)
	{
		// Single-point patrol fallback (or null if designer didn't set any point).
		return PatrolTarget;
	}

	TArray<AActor*> ValidTargets;
	for (AActor* Target : PatrolTargets)
	{
		if (Target != PatrolTarget)
		{
			ValidTargets.AddUnique(Target);
		}
	}

	const int32 NumPatrolTargets = ValidTargets.Num();
	if (NumPatrolTargets > 0)
	{
		const int32 TargetSelection = FMath::RandRange(0, NumPatrolTargets - 1);
		return ValidTargets[TargetSelection];
	}
	return PatrolTarget ? PatrolTarget : PatrolTargets[0];
}

void AEnemy::SpawnDefaultWeapon()
{
	UWorld* World = GetWorld();
	if (World && WeaponClass)
	{
		AWeapon* DefaultWeapon = World->SpawnActor<AWeapon>(WeaponClass);
		DefaultWeapon->Equip(GetMesh(), FName("RightHandSocket"), this, this);
		EquippedWeapon = DefaultWeapon;
	}
}

void AEnemy::PawnSeen(APawn* SeenPawn)
{
	const bool bShouldChaseTarget =
		EnemyState != EEnemyState::EES_Dead &&
		EnemyState != EEnemyState::EES_Chasing &&
		EnemyState < EEnemyState::EES_Attacking &&
		SeenPawn->ActorHasTag(FName("EngageableTarget"));

	if (bShouldChaseTarget)
	{
		CombatTarget = SeenPawn;
		ClearPatrolTimer();
		if (IsUsingBehaviorTree())
		{
			EnterChasingStateFromBT();
			ShowHealthBar();
		}
		else
		{
			ChaseTarget();
		}
	}
}

bool AEnemy::CanAttackFromBT()
{
	return CanAttack();
}

void AEnemy::ExecuteAttackFromBT()
{
	Attack();
}

void AEnemy::EnterChasingStateFromBT()
{
	EnemyState = EEnemyState::EES_Chasing;
	GetCharacterMovement()->MaxWalkSpeed = ChasingSpeed;
}

void AEnemy::EnterPatrollingStateFromBT()
{
	EnemyState = EEnemyState::EES_Patrolling;
	GetCharacterMovement()->MaxWalkSpeed = PatrollingSpeed;
}

AActor* AEnemy::ChooseNextPatrolTarget()
{
	return ChoosePatrolTarget();
}

bool AEnemy::IsTargetOutsideCombatRangeFromBT()
{
	return IsOutsideCombatRadius();
}

void AEnemy::ClearCombatTargetFromBT()
{
	LoseInterest();
	EnterPatrollingStateFromBT();
}






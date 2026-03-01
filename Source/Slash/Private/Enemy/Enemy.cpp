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
#include "BehaviorTree/BlackboardComponent.h"
#include "Soul.h"
#include "Weapons/Weapon.h"
#include "Slash/Slash.h"
#include "Net/UnrealNetwork.h"

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

void AEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEnemy, EnemyState);
}

void AEnemy::OnRep_EnemyState()
{
	if (!bEnableAIStateLog) return;

	const UEnum* EnemyStateEnum = StaticEnum<EEnemyState>();
	const FString StateStr = EnemyStateEnum
		? EnemyStateEnum->GetNameStringByValue(static_cast<int64>(EnemyState))
		: TEXT("Unknown");

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[AIStateRep][%s] State=%s NetMode=%s Auth=%d"),
		*GetName(),
		*StateStr,
		*ToString(GetNetMode()),
		HasAuthority() ? 1 : 0
	);
}

void AEnemy::SetEnemyState(EEnemyState NewState, const TCHAR* Reason)
{
	// Replicated state must be server-authored; client-side writes cause state divergence
	// (e.g. Dead being overwritten back to NoState by local anim notify).
	if (!HasAuthority()) return;

	// Death is terminal for this actor instance; do not allow state rollback.
	if (EnemyState == EEnemyState::EES_Dead && NewState != EEnemyState::EES_Dead)
	{
		if (bEnableAIStateLog)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[AIState][%s] Ignore %s while Dead. Reason=%s"),
				*GetName(),
				*StaticEnum<EEnemyState>()->GetNameStringByValue(static_cast<int64>(NewState)),
				Reason ? Reason : TEXT("Unknown")
			);
		}
		return;
	}

	if (EnemyState == NewState) return;

	if (bEnableAIStateLog)
	{
		const UEnum* EnemyStateEnum = StaticEnum<EEnemyState>();
		const FString OldState = EnemyStateEnum ? EnemyStateEnum->GetNameStringByValue(static_cast<int64>(EnemyState)) : TEXT("Unknown");
		const FString NextState = EnemyStateEnum ? EnemyStateEnum->GetNameStringByValue(static_cast<int64>(NewState)) : TEXT("Unknown");
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AIState][%s] %s -> %s Reason=%s NetMode=%s Auth=%d"),
			*GetName(),
			*OldState,
			*NextState,
			Reason ? Reason : TEXT("Unknown"),
			*ToString(GetNetMode()),
			HasAuthority() ? 1 : 0
		);
	}

	EnemyState = NewState;
}

void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bEnableAIRealtimeStateLog)
	{
		AIRealtimeStateLogAccumulator += DeltaTime;
		if (AIRealtimeStateLogAccumulator >= AIRealtimeStateLogInterval)
		{
			AIRealtimeStateLogAccumulator = 0.f;
			const UEnum* EnemyStateEnum = StaticEnum<EEnemyState>();
			const FString StateStr = EnemyStateEnum
				? EnemyStateEnum->GetNameStringByValue(static_cast<int64>(EnemyState))
				: TEXT("Unknown");
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[AIStateLive][%s] State=%s NetMode=%s Auth=%d Target=%s"),
				*GetName(),
				*StateStr,
				*ToString(GetNetMode()),
				HasAuthority() ? 1 : 0,
				CombatTarget ? *CombatTarget->GetName() : TEXT("None")
			);
		}
	}

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

	// Server-authoritative death gate: in BT mode, drive death through blackboard/task;
	// in non-BT mode, use direct Die() fallback.
	if (ShouldDieFromBT())
	{
		if (IsUsingBehaviorTree())
		{
			if (AEnemyAIController* BTController = Cast<AEnemyAIController>(EnemyController))
			{
				if (UBlackboardComponent* BBComp = BTController->GetBlackboardComponent())
				{
					BBComp->SetValueAsBool(AEnemyAIController::IsDeadKeyName, true);
				}
			}
		}
		else if (!IsDead())
		{
			Die();
		}
		return DamageAmount;
	}

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
		SetEnemyState(EEnemyState::EES_Attacking, TEXT("TakeDamage_InAttackRadius"));
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
	// Stop current attack montage first so hit-react montage can take over immediately.
	StopAttackMontage();
	Super::GetHit_Implementation(ImpactPoint, Hitter);
	if (IsDead()) return;
	if(!IsDead())ShowHealthBar();
	ClearPatrolTimer();
	ClearAttackTimer();
	SetWeaponCollisionEnabled(ECollisionEnabled::NoCollision);

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

	UE_LOG(LogTemp, Warning, TEXT("[EnemyInit][%s] NetMode=%s Role=%s Auth=%d UseBT=%d"),
		*GetName(), *NetModeStr, *RoleStr, HasAuthority() ? 1 : 0, IsUsingBehaviorTree() ? 1 : 0);


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

	// Lethal hit can arrive while attack montage is playing; stop it first
	// so death montage/state can take over immediately.
	StopAttackMontage();

	Super::Die();
	SetEnemyState(EEnemyState::EES_Dead, TEXT("Die"));
	ClearAttackTimer();
	ClearPatrolTimer();
	HideHealthBar();
	// Keep corpse for debugging death montage/state-machine transitions.
	SetLifeSpan(0.f);
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
		SetEnemyState(EEnemyState::EES_Engaged, TEXT("Attack_PlayMontage"));
	}
	else
	{
		// No valid attack montage section: keep AI from getting stuck in engaged state.
		SetEnemyState(EEnemyState::EES_NoState, TEXT("Attack_NoValidMontageSection"));
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
	if (!HasAuthority()) return;
	if (IsDead()) return;

	SetEnemyState(EEnemyState::EES_NoState, TEXT("AttackEnd"));
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
			SetEnemyState(EEnemyState::EES_Patrolling, TEXT("BT_Initialize"));
			GetCharacterMovement()->MaxWalkSpeed = PatrollingSpeed;
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
	SetEnemyState(EEnemyState::EES_Patrolling, TEXT("StartPatrolling"));
	GetCharacterMovement()->MaxWalkSpeed = PatrollingSpeed;
	MoveToTarget(PatrolTarget);
}

void AEnemy::ChaseTarget()
{
	SetEnemyState(EEnemyState::EES_Chasing, TEXT("ChaseTarget"));
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
	SetEnemyState(EEnemyState::EES_Attacking, TEXT("StartAttackTimer"));
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
		return PatrolTarget;
	}

	TArray<AActor*> UniqueTargets;
	for (AActor* Target : PatrolTargets)
	{
		if (IsValid(Target))
		{
			UniqueTargets.AddUnique(Target);
		}
	}

	if (UniqueTargets.Num() <= 0)
	{
		return PatrolTarget;
	}

	// Prefer changing destination when possible.
	TArray<AActor*> CandidateTargets = UniqueTargets;
	if (IsValid(PatrolTarget))
	{
		CandidateTargets.Remove(PatrolTarget);
	}
	if (CandidateTargets.Num() <= 0)
	{
		CandidateTargets = UniqueTargets;
	}

	const int32 Selection = FMath::RandRange(0, CandidateTargets.Num() - 1);
	AActor* NextTarget = CandidateTargets[Selection];
	if (bEnableAIStateLog)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AIPatrol][%s] Current=%s Next=%s Candidates=%d"),
			*GetName(),
			PatrolTarget ? *PatrolTarget->GetName() : TEXT("None"),
			NextTarget ? *NextTarget->GetName() : TEXT("None"),
			CandidateTargets.Num()
		);
	}
	return NextTarget;
}

void AEnemy::SpawnDefaultWeapon()
{
	if (!HasAuthority()) return;
	if (EquippedWeapon != nullptr) return;

	UWorld* World = GetWorld();
	if (World && WeaponClass)
	{
		AWeapon* DefaultWeapon = World->SpawnActor<AWeapon>(WeaponClass);
		if (DefaultWeapon)
		{
			DefaultWeapon->Equip(GetMesh(), FName("RightHandSocket"), this, this);
			EquippedWeapon = DefaultWeapon;
		}
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

bool AEnemy::ShouldDieFromBT() const
{
	return Attributes && !Attributes->IsAlive();
}

void AEnemy::ExecuteDeathFromBT()
{
	if (!ShouldDieFromBT()) return;
	if (IsDead()) return;
	Die();
}

void AEnemy::EnterChasingStateFromBT()
{
	SetEnemyState(EEnemyState::EES_Chasing, TEXT("BT_EnterChasing"));
	GetCharacterMovement()->MaxWalkSpeed = ChasingSpeed;
}

void AEnemy::EnterPatrollingStateFromBT()
{
	SetEnemyState(EEnemyState::EES_Patrolling, TEXT("BT_EnterPatrolling"));
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








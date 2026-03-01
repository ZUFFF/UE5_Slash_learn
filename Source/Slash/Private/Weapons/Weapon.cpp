// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapons/Weapon.h"
#include "SlashCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Interfaces/HitInterface.h"
#include "NiagaraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Net/UnrealNetwork.h"

AWeapon::AWeapon()
{
	bReplicates = true;
	SetReplicateMovement(true);

	WeaponBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Weapon Box"));
	WeaponBox->SetupAttachment(GetRootComponent());
	WeaponBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	WeaponBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	BoxTraceStart = CreateDefaultSubobject<USceneComponent>(TEXT("Box Trace Start"));
	BoxTraceStart->SetupAttachment(GetRootComponent());

	BoxTraceEnd = CreateDefaultSubobject<USceneComponent>(TEXT("Box Trace End"));
	BoxTraceEnd->SetupAttachment(GetRootComponent());
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeapon, bEmbersActive);
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	WeaponBox->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::OnBoxOverlap);
	ApplyEmbersState();
}
void AWeapon::Equip(USceneComponent* InParent, FName InSocketName, AActor* NewOwner, APawn* NewInstigator)
{
	ItemState = EItemState::EIS_Equipped;
	SetOwner(NewOwner);
	SetInstigator(NewInstigator);
	AttachMeshToSocket(InParent, InSocketName);
	DisableSphereCollision();
	PlayEquipSound();
	DeactivateEmbers();
}

void AWeapon::DeactivateEmbers()
{
	SetEmbersActive(false);
}

void AWeapon::SetEmbersActive(bool bNewActive)
{
	if (HasAuthority())
	{
		bEmbersActive = bNewActive;
	}
	else
	{
		// Local fallback for non-network scenarios.
		bEmbersActive = bNewActive;
	}

	ApplyEmbersState();
}

void AWeapon::ApplyEmbersState()
{
	if (!ItemEffect) return;

	if (bEmbersActive)
	{
		ItemEffect->Activate(true);
	}
	else
	{
		ItemEffect->Deactivate();
	}
}

void AWeapon::OnRep_EmbersActive()
{
	ApplyEmbersState();
}

void AWeapon::DisableSphereCollision()
{
	if (Sphere)
	{
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AWeapon::PlayEquipSound()
{
	if (EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			EquipSound,
			GetActorLocation()
		);
	}
}

void AWeapon::AttachMeshToSocket(USceneComponent* InParent, const FName& InSocketName)
{
	FAttachmentTransformRules TransformRules(EAttachmentRule::SnapToTarget, true);
	ItemMesh->AttachToComponent(InParent, TransformRules, InSocketName);
}

AProjectile* AWeapon::SpawnProjectile()
{
	return GetWorld()->SpawnActor<AProjectile>(ProjectileClass, GetActorLocation(), GetActorRotation());
}


void AWeapon::OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[HitTrace][%s] OnBoxOverlap Other=%s Owner=%s Auth=%d"),
		*GetName(),
		*GetNameSafe(OtherActor),
		*GetNameSafe(GetOwner()),
		HasAuthority() ? 1 : 0
	);

	//if (ActorIsSameType(OtherActor)) return;

	//FHitResult BoxHit;
	//BoxTrace(BoxHit);

	//if (BoxHit.GetActor())
	//{
	//	if (ActorIsSameType(BoxHit.GetActor())) return;

	//	UGameplayStatics::ApplyDamage(BoxHit.GetActor(), Damage, GetInstigator()->GetController(), this, UDamageType::StaticClass());
	//	ExecuteGetHit(BoxHit);
	//	CreateFields(BoxHit.ImpactPoint);
	//}
	if (bShowBoxDebug)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[WeaponHitTrace][%s] Overlap Other=%s Owner=%s Auth=%d"),
			*GetName(),
			*GetNameSafe(OtherActor),
			*GetNameSafe(GetOwner()),
			HasAuthority() ? 1 : 0
		);
	}

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] Abort: NoAuthority"), *GetName());
		if (bShowBoxDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WeaponHitTrace][%s] Skip: NoAuthority"), *GetName());
		}
		return;
	}

	if (!OtherActor || OtherActor == GetOwner())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] Abort: InvalidOrOwner Other=%s"), *GetName(), *GetNameSafe(OtherActor));
		if (bShowBoxDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WeaponHitTrace][%s] Skip: InvalidOrOwner"), *GetName());
		}
		return;
	}

	if (ActorIsSameType(OtherActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] Abort: SameType Other=%s"), *GetName(), *GetNameSafe(OtherActor));
		if (bShowBoxDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WeaponHitTrace][%s] Skip: SameType OwnerTagEnemy=%d OtherTagEnemy=%d"),
				*GetName(),
				GetOwner() && GetOwner()->ActorHasTag(TEXT("Enemy")) ? 1 : 0,
				OtherActor->ActorHasTag(TEXT("Enemy")) ? 1 : 0);
		}
		return;
	}

	if (!OtherActor->GetClass()->ImplementsInterface(UHitInterface::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] Abort: TargetNoHitInterface Target=%s"), *GetName(), *GetNameSafe(OtherActor));
		if (bShowBoxDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WeaponHitTrace][%s] Skip: NotHitInterface Target=%s"),
				*GetName(), *GetNameSafe(OtherActor));
		}
		return;
	}

	WeaponBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FVector FieldLocation = SweepResult.bBlockingHit ? SweepResult.ImpactPoint : GetActorLocation();
	MulticastCreateFields(FieldLocation);
	UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] ApplyDamage Target=%s Damage=%.2f"), *GetName(), *GetNameSafe(OtherActor), Damage);
	UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigator()->GetController(), this, UDamageType::StaticClass());
	UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] Execute_GetHit Target=%s"), *GetName(), *GetNameSafe(OtherActor));
	ExecuteGetHit(OtherActor);
	UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] Execute_GetHit Done Target=%s"), *GetName(), *GetNameSafe(OtherActor));

	if (bShowBoxDebug)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponHitTrace][%s] ApplyDamage+ExecuteGetHit Target=%s Damage=%.2f"),
			*GetName(),
			*GetNameSafe(OtherActor),
			Damage);
	}
	if (GetOwner()->ActorHasTag(TEXT("EngageableTarget")))
	{
		ASlashCharacter* Character = Cast< ASlashCharacter>(GetOwner());
		Character->StartHitReaction();
	}
	

}

void AWeapon::MulticastCreateFields_Implementation(const FVector_NetQuantize& FieldLocation)
{
	CreateFields(FVector(FieldLocation));
}

bool AWeapon::ActorIsSameType(AActor* OtherActor)
{
	return GetOwner()->ActorHasTag(TEXT("Enemy")) && OtherActor->ActorHasTag(TEXT("Enemy"));
}

void AWeapon::ExecuteGetHit(FHitResult& BoxHit)
{
	IHitInterface* HitInterface = Cast<IHitInterface>(BoxHit.GetActor());
	if (HitInterface)
	{
		AActor* HitterActor = GetOwner();
		if (!HitterActor) HitterActor = GetInstigator();
		if (!HitterActor) HitterActor = this;
		HitInterface->Execute_GetHit(BoxHit.GetActor(), BoxHit.ImpactPoint, HitterActor);
	}
}

void AWeapon::ExecuteGetHit(AActor* OtherActor)
{
	if (!OtherActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] ExecuteGetHit Abort: OtherActor null"), *GetName());
		return;
	}

	const bool bImplements = OtherActor->GetClass()->ImplementsInterface(UHitInterface::StaticClass());
	IHitInterface* HitInterface = Cast<IHitInterface>(OtherActor);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[HitTrace][%s] ExecuteGetHit Target=%s Implements=%d CastOK=%d"),
		*GetName(),
		*GetNameSafe(OtherActor),
		bImplements ? 1 : 0,
		HitInterface ? 1 : 0
	);
	if (HitInterface)
	{
		AActor* HitterActor = GetOwner();
		if (!HitterActor) HitterActor = GetInstigator();
		if (!HitterActor) HitterActor = this;
		UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] Execute_GetHit Call Hitter=%s"), *GetName(), *GetNameSafe(HitterActor));
		HitInterface->Execute_GetHit(OtherActor, GetActorLocation(), HitterActor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitTrace][%s] ExecuteGetHit Abort: Cast<IHitInterface> failed"), *GetName());
	}
}

void AWeapon::BoxTrace(FHitResult& BoxHit)
{
	const FVector Start = BoxTraceStart->GetComponentLocation();
	const FVector End = BoxTraceEnd->GetComponentLocation();

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	ActorsToIgnore.Add(GetOwner());

	for (AActor* Actor : IgnoreActors)
	{
		ActorsToIgnore.AddUnique(Actor);
	}

	UKismetSystemLibrary::BoxTraceSingle(
		this,
		Start,
		End,
		BoxTraceExtent,
		BoxTraceStart->GetComponentRotation(),
		ETraceTypeQuery::TraceTypeQuery1,
		false,
		ActorsToIgnore,
		bShowBoxDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		BoxHit,
		true
	);
	IgnoreActors.AddUnique(BoxHit.GetActor());
}

void AWeapon::Fire(const FVector& HitTarget, USkeletalMeshComponent* OwnerMesh, FName SocketName)
{
	
	if (FireAnimation)
	{
		//ItemMesh->PlayAnimation(FireAnimation, false);
	}
	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	const USkeletalMeshSocket* FireSocket = OwnerMesh->GetSocketByName(SocketName);
	if (FireSocket)
	{
		FTransform SocketTransform = FireSocket->GetSocketTransform(OwnerMesh);
		// From socket to hit location from TraceUnderCrosshairs
		FVector ToTarget = HitTarget - SocketTransform.GetLocation();
		FRotator TargetRotation = ToTarget.Rotation();
		if (ProjectileClass && InstigatorPawn)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();
			SpawnParams.Instigator = InstigatorPawn;
			UWorld* World = GetWorld();
			if (World)
			{
				World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
			}
		}

	}
	
}

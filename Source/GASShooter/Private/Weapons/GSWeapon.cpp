// Copyright 2020 Dan Kestranek.


#include "Weapons/GSWeapon.h"
#include "Characters/Abilities/GSAbilitySystemComponent.h"
#include "Characters/Abilities/GSAbilitySystemGlobals.h"
#include "Characters/Abilities/GSGameplayAbility.h"
#include "Characters/Abilities/GSGATA_LineTrace.h"
#include "Characters/Abilities/GSGATA_SphereTrace.h"
#include "Characters/Heroes/GSHeroCharacter.h"
#include "Weapons/GSUTProjectile.h"
#include "Player/GSPlayerController.h"
#include "GSBlueprintFunctionLibrary.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "PaperSprite.h"

// Sets default values
AGSWeapon::AGSWeapon()
{
    // Set this actor to never tick
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    bNetUseOwnerRelevancy = true;
    NetUpdateFrequency = 100.0f; // Set this to a value that's appropriate for your game
    bSpawnWithCollision = true;
    PrimaryClipAmmo = 0;
    MaxPrimaryClipAmmo = 0;
    SecondaryClipAmmo = 0;
    MaxSecondaryClipAmmo = 0;
    bInfiniteAmmo = false;
    PrimaryAmmoType = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.None"));
    SecondaryAmmoType = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.None"));

    FiringNoiseLoudness = 1.f;
    FiringNoiseMaxRange = 2000.f;

    FireRate = 0.1f;

    CollisionComp = CreateDefaultSubobject<UCapsuleComponent>(FName("CollisionComponent"));
    CollisionComp->InitCapsuleSize(40.0f, 50.0f);
    CollisionComp->SetCollisionObjectType(COLLISION_PICKUP);
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Manually enable when in pickup mode
    CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    RootComponent = CollisionComp;

    WeaponMesh3PickupRelativeLocation = FVector(0.0f, -25.0f, 0.0f);

    WeaponMesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(FName("WeaponMesh3P"));
    WeaponMesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeaponMesh3P->SetupAttachment(CollisionComp);
    WeaponMesh3P->SetRelativeLocation(WeaponMesh3PickupRelativeLocation);
    WeaponMesh3P->CastShadow = true;
    WeaponMesh3P->SetVisibility(true, true);
    WeaponMesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

    WeaponPrimaryInstantAbilityTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.Primary.Instant");
    WeaponSecondaryInstantAbilityTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.Secondary.Instant");
    WeaponAlternateInstantAbilityTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.Alternate.Instant");
    WeaponIsFiringTag = FGameplayTag::RequestGameplayTag("Weapon.IsFiring");

    FireMode = FGameplayTag::RequestGameplayTag("Weapon.FireMode.None");
    StatusText = DefaultStatusText;

    RestrictedPickupTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
    RestrictedPickupTags.AddTag(FGameplayTag::RequestGameplayTag("State.KnockedDown"));
}

UAbilitySystemComponent* AGSWeapon::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

USkeletalMeshComponent* AGSWeapon::GetWeaponMesh3P() const
{
    return WeaponMesh3P;
}

UAnimMontage* AGSWeapon::GetWeaponFiringAnimation() const
{
    return WeaponFiringAnimation;
}

UAnimMontage* AGSWeapon::GetWeaponReloadAnimation() const
{
    return WeaponReloadAnimation;
}

UAnimMontage* AGSWeapon::GetCharacterFiringAnimation() const
{
    return CharacterFiringAnimation;
}

UAnimMontage* AGSWeapon::GetCharacterReloadAnimation() const
{
    return CharacterReloadAnimation;
}

FName AGSWeapon::GetMuzzleSocketName() const
{
    return MuzzleSocketName;
}

void AGSWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(AGSWeapon, OwningCharacter, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AGSWeapon, PrimaryClipAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AGSWeapon, MaxPrimaryClipAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AGSWeapon, SecondaryClipAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AGSWeapon, MaxSecondaryClipAmmo, COND_OwnerOnly);
}

void AGSWeapon::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
    Super::PreReplication(ChangedPropertyTracker);

    DOREPLIFETIME_ACTIVE_OVERRIDE(AGSWeapon, PrimaryClipAmmo, (IsValid(AbilitySystemComponent) && !AbilitySystemComponent->HasMatchingGameplayTag(WeaponIsFiringTag)));
    DOREPLIFETIME_ACTIVE_OVERRIDE(AGSWeapon, SecondaryClipAmmo, (IsValid(AbilitySystemComponent) && !AbilitySystemComponent->HasMatchingGameplayTag(WeaponIsFiringTag)));
}

void AGSWeapon::SetOwningCharacter(AGSHeroCharacter* InOwningCharacter)
{
    OwningCharacter = InOwningCharacter;

    if (OwningCharacter)
    {
        // Called when added to inventory
        AbilitySystemComponent = Cast<UGSAbilitySystemComponent>(OwningCharacter->GetAbilitySystemComponent());
        SetOwner(InOwningCharacter);
        AttachToComponent(OwningCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        if (OwningCharacter->GetCurrentWeapon() != this)
        {
            WeaponMesh3P->CastShadow = false;
            WeaponMesh3P->SetVisibility(true, true);
            WeaponMesh3P->SetVisibility(false, true);
        }
    }
    else
    {
        AbilitySystemComponent = nullptr;
        SetOwner(nullptr);
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }
}

void AGSWeapon::NotifyActorBeginOverlap(AActor* Other)
{
    Super::NotifyActorBeginOverlap(Other);

    if (!IsPendingKill() && !OwningCharacter)
    {
        PickUpOnTouch(Cast<AGSHeroCharacter>(Other));
    }
}

void AGSWeapon::Equip()
{
    if (!OwningCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("%s %s OwningCharacter is nullptr"), *FString(__FUNCTION__), *GetName());
        return;
    }

    FName AttachPoint = OwningCharacter->GetWeaponAttachPoint();

    if (WeaponMesh3P)
    {
        WeaponMesh3P->AttachToComponent(OwningCharacter->GetMainMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, AttachPoint);
        WeaponMesh3P->SetRelativeLocation(WeaponMesh3PEquippedRelativeLocation);
        WeaponMesh3P->CastShadow = true;
        WeaponMesh3P->bCastHiddenShadow = true;

        WeaponMesh3P->SetVisibility(true, true);
    }
}

void AGSWeapon::UnEquip()
{
    if (OwningCharacter == nullptr)
    {
        return;
    }

    // Necessary to detach so that when toggling perspective all meshes attached won't become visible.

    WeaponMesh3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
    WeaponMesh3P->CastShadow = false;
    WeaponMesh3P->bCastHiddenShadow = false;
    WeaponMesh3P->SetVisibility(true, true); // Without this, the unequipped weapon's 3p shadow hangs around
    WeaponMesh3P->SetVisibility(false, true);
}

void AGSWeapon::AddAbilities()
{
    // Grant abilities, but only on the server  
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    if (! IsValid(AbilitySystemComponent))
    {
        return;
    }

    for (TSubclassOf<UGSGameplayAbility>& Ability : Abilities)
    {
        AbilitySpecHandles.Add(
            AbilitySystemComponent->GiveAbility(
                FGameplayAbilitySpec(
                    Ability,
                    GetAbilityLevel(Ability.GetDefaultObject()->AbilityID),
                    static_cast<int32>(Ability.GetDefaultObject()->AbilityInputID),
                    this
                    )
                )
            );
    }
}

void AGSWeapon::RemoveAbilities()
{
    // Remove abilities, but only on the server 
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    if (! IsValid(AbilitySystemComponent))
    {
        return;
    }

    for (FGameplayAbilitySpecHandle& SpecHandle : AbilitySpecHandles)
    {
        AbilitySystemComponent->ClearAbility(SpecHandle);
    }
}

int32 AGSWeapon::GetAbilityLevel(EGSAbilityInputID AbilityID)
{
    // All abilities for now are level 1
    return 1;
}

void AGSWeapon::ResetWeapon()
{
    FireMode = DefaultFireMode;
    StatusText = DefaultStatusText;
}

void AGSWeapon::OnDropped_Implementation(FVector NewLocation)
{
    SetOwningCharacter(nullptr);
    ResetWeapon();

    SetActorLocation(NewLocation);
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    if (WeaponMesh3P)
    {
        WeaponMesh3P->AttachToComponent(CollisionComp, FAttachmentTransformRules::SnapToTargetIncludingScale);
        WeaponMesh3P->SetRelativeLocation(WeaponMesh3PickupRelativeLocation);
        WeaponMesh3P->CastShadow = true;
        WeaponMesh3P->SetVisibility(true, true);
    }
}

bool AGSWeapon::OnDropped_Validate(FVector NewLocation)
{
    return true;
}

int32 AGSWeapon::GetPrimaryClipAmmo() const
{
    return PrimaryClipAmmo;
}

int32 AGSWeapon::GetMaxPrimaryClipAmmo() const
{
    return MaxPrimaryClipAmmo;
}

int32 AGSWeapon::GetSecondaryClipAmmo() const
{
    return SecondaryClipAmmo;
}

int32 AGSWeapon::GetMaxSecondaryClipAmmo() const
{
    return MaxSecondaryClipAmmo;
}

void AGSWeapon::SetPrimaryClipAmmo(int32 NewPrimaryClipAmmo)
{
    int32 OldPrimaryClipAmmo = PrimaryClipAmmo;
    PrimaryClipAmmo = NewPrimaryClipAmmo;
    OnPrimaryClipAmmoChanged.Broadcast(OldPrimaryClipAmmo, PrimaryClipAmmo);
}

void AGSWeapon::SetMaxPrimaryClipAmmo(int32 NewMaxPrimaryClipAmmo)
{
    int32 OldMaxPrimaryClipAmmo = MaxPrimaryClipAmmo;
    MaxPrimaryClipAmmo = NewMaxPrimaryClipAmmo;
    OnMaxPrimaryClipAmmoChanged.Broadcast(OldMaxPrimaryClipAmmo, MaxPrimaryClipAmmo);
}

void AGSWeapon::SetSecondaryClipAmmo(int32 NewSecondaryClipAmmo)
{
    int32 OldSecondaryClipAmmo = SecondaryClipAmmo;
    SecondaryClipAmmo = NewSecondaryClipAmmo;
    OnSecondaryClipAmmoChanged.Broadcast(OldSecondaryClipAmmo, SecondaryClipAmmo);
}

void AGSWeapon::SetMaxSecondaryClipAmmo(int32 NewMaxSecondaryClipAmmo)
{
    int32 OldMaxSecondaryClipAmmo = MaxSecondaryClipAmmo;
    MaxSecondaryClipAmmo = NewMaxSecondaryClipAmmo;
    OnMaxSecondaryClipAmmoChanged.Broadcast(OldMaxSecondaryClipAmmo, MaxSecondaryClipAmmo);
}

TSubclassOf<UGSHUDReticle> AGSWeapon::GetPrimaryHUDReticleClass() const
{
    return PrimaryHUDReticleClass;
}

bool AGSWeapon::HasInfiniteAmmo() const
{
    return bInfiniteAmmo;
}

float AGSWeapon::GetFiringNoiseLoudness() const
{
    return FiringNoiseLoudness;
}

float AGSWeapon::GetFiringNoiseMaxRange() const
{
    return FiringNoiseMaxRange;
}

UAnimMontage* AGSWeapon::GetEquip3PMontage() const
{
    return Equip3PMontage;
}

USoundCue* AGSWeapon::GetPickupSound() const
{
    return PickupSound;
}

FText AGSWeapon::GetDefaultStatusText() const
{
    return DefaultStatusText;
}

AGSGATA_LineTrace* AGSWeapon::GetLineTraceTargetActor()
{
    if (LineTraceTargetActor)
    {
        return LineTraceTargetActor;
    }

    LineTraceTargetActor = GetWorld()->SpawnActor<AGSGATA_LineTrace>();
    LineTraceTargetActor->SetOwner(this);
    return LineTraceTargetActor;
}

AGSGATA_SphereTrace* AGSWeapon::GetSphereTraceTargetActor()
{
    if (SphereTraceTargetActor)
    {
        return SphereTraceTargetActor;
    }

    SphereTraceTargetActor = GetWorld()->SpawnActor<AGSGATA_SphereTrace>();
    SphereTraceTargetActor->SetOwner(this);
    return SphereTraceTargetActor;
}

void AGSWeapon::BeginPlay()
{
    ResetWeapon();

    if (!OwningCharacter && bSpawnWithCollision)
    {
        // Spawned into the world without an owner, enable collision as we are in pickup mode
        CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    Super::BeginPlay();
}

void AGSWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    if (LineTraceTargetActor)
    {
        LineTraceTargetActor->Destroy();
    }

    if (SphereTraceTargetActor)
    {
        SphereTraceTargetActor->Destroy();
    }

    Super::EndPlay(EndPlayReason);
}

void AGSWeapon::PickUpOnTouch(AGSHeroCharacter* InCharacter)
{
    if (!InCharacter ||
        !InCharacter->IsAlive() ||
        !InCharacter->GetAbilitySystemComponent() ||
        InCharacter->GetAbilitySystemComponent()->HasAnyMatchingGameplayTags(RestrictedPickupTags))
    {
        return;
    }

    InCharacter->AddWeaponToInventory(this, true);
}

FGSProjectileSpawnInfo AGSWeapon::FireProjectile(
    TSubclassOf<AGSUTProjectile> ProjectileType,
    FVector ProjectileLocation,
    FRotator ProjectileRotation,
    bool bDeferForwardTick
    )
{
    //UE_LOG(LogTemp, Verbose, TEXT("%s::FireProjectile()"), *GetName());

    if (! IsValid(OwningCharacter))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s::FireProjectile(): Weapon is not owned (owner died during firing sequence)"), *GetName());
        return {};
    }

    //if (Role == ROLE_Authority)
    //{
    //    UTOwner->IncrementFlashCount(CurrentFireMode);
    //    AUTPlayerState* PS = UTOwner->Controller ? Cast<AUTPlayerState>(UTOwner->Controller->PlayerState) : NULL;
    //    if (PS && (ShotsStatsName != NAME_None))
    //    {
    //        PS->ModifyStatsValue(ShotsStatsName, 1);
    //    }
    //}

    // spawn the projectile at the muzzle
    //const FVector SpawnLocation = GetFireStartLoc();
    //const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
    return SpawnNetPredictedProjectile(
        ProjectileType,
        ProjectileLocation,
        ProjectileRotation,
        bDeferForwardTick
        );
}

FGSProjectileSpawnInfo AGSWeapon::SpawnNetPredictedProjectile(
    TSubclassOf<AGSUTProjectile> ProjectileClass,
    FVector SpawnLocation,
    FRotator SpawnRotation,
    bool bDeferForwardTick
    )
{
    //DrawDebugSphere(GetWorld(), SpawnLocation, 10, 10, FColor::Green, true);

    const bool bHasAuthority = GetLocalRole() == ROLE_Authority;

    AGSPlayerController* OwningPlayer = OwningCharacter
        ? Cast<AGSPlayerController>(OwningCharacter->GetController())
        : NULL;

    float CatchupTickDelta = 
        ((GetNetMode() != NM_Standalone) && OwningPlayer)
        ? OwningPlayer->GetPredictionTime()
        : 0.f;

    //if (OwningPlayer)
    //{
    //    float CurrentMoveTime = (UTOwner && UTOwner->UTCharacterMovement) ? UTOwner->UTCharacterMovement->GetCurrentSynchTime() : GetWorld()->GetTimeSeconds();
    //    if (UTOwner->Role < ROLE_Authority)
    //    {
    //        UE_LOG(UT, Warning, TEXT("CLIENT SpawnNetPredictedProjectile at %f yaw %f "), CurrentMoveTime, SpawnRotation.Yaw);
    //    }
    //    else
    //    {
    //        UE_LOG(UT, Warning, TEXT("SERVER SpawnNetPredictedProjectile at %f yaw %f TIME %f"), CurrentMoveTime, SpawnRotation.Yaw, GetWorld()->GetTimeSeconds());
    //    }
    //}

    //if (!bHasAuthority)
    //    UE_LOG(LogTemp, Warning, TEXT("AUTH: %d, CatchupTick: %f, Ping: %f, ExactPing: %f, MaxPredictionPing: %f"),
    //        bHasAuthority,
    //        CatchupTickDelta*1000.f,
    //        OwningPlayer?(OwningPlayer->PlayerState?OwningPlayer->PlayerState->GetPing():-1.f):-1.f,
    //        OwningPlayer?(OwningPlayer->PlayerState?OwningPlayer->PlayerState->ExactPing:-1.f):-1.f,
    //        OwningPlayer?OwningPlayer->GetMaxPredictionPing():-1.f
    //        );

    if ((CatchupTickDelta > 0.f) && !bHasAuthority)
    {
        float SleepTime = OwningPlayer->GetProjectileSleepTime();

        if (SleepTime > 0.f)
        {
            // lag is so high need to delay spawn
            if (! GetWorldTimerManager().IsTimerActive(SpawnDelayedFakeProjHandle))
            {
                DelayedProjectile.ProjectileClass = ProjectileClass;
                DelayedProjectile.SpawnLocation = SpawnLocation;
                DelayedProjectile.SpawnRotation = SpawnRotation;
                GetWorldTimerManager().SetTimer(
                    SpawnDelayedFakeProjHandle,
                    this,
                    &AGSWeapon::SpawnDelayedFakeProjectile,
                    SleepTime,
                    false
                    );
            }

            return FGSProjectileSpawnInfo(
                nullptr,
                0.f,
                false
                );
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Instigator = OwningCharacter;
    SpawnParams.Owner = OwningCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGSUTProjectile* NewProjectile = (bHasAuthority || (CatchupTickDelta > 0.f))
        ? GetWorld()->SpawnActor<AGSUTProjectile>(
            ProjectileClass,
            SpawnLocation,
            SpawnRotation,
            SpawnParams
            )
        : NULL;

    if (NewProjectile)
    {
        //if (NewProjectile->OffsetVisualComponent)
        //{
        //    switch (GetWeaponHand())
        //    {
        //        case EWeaponHand::HAND_Center:
        //            NewProjectile->InitialVisualOffset = NewProjectile->InitialVisualOffset + LowMeshOffset;
        //            NewProjectile->OffsetVisualComponent->RelativeLocation = NewProjectile->InitialVisualOffset;
        //            break;
        //        case EWeaponHand::HAND_Hidden:
        //        {
        //            NewProjectile->InitialVisualOffset = NewProjectile->InitialVisualOffset + VeryLowMeshOffset;
        //            NewProjectile->OffsetVisualComponent->RelativeLocation = NewProjectile->InitialVisualOffset;
        //            break;
        //        }
        //    }
        //}

        if (OwningCharacter)
        {
            //OwningCharacter->LastFiredProjectile = NewProjectile;
            NewProjectile->ShooterLocation = OwningCharacter->GetActorLocation();
            NewProjectile->ShooterRotation = OwningCharacter->GetActorRotation();
        }

        if (bHasAuthority)
        {
            //NewProjectile->HitsStatsName = HitsStatsName;

            if (! bDeferForwardTick)
            {
                ForwardTickProjectile(NewProjectile, CatchupTickDelta);
            }
        }
        else
        {
            NewProjectile->InitFakeProjectile(OwningPlayer);
            //NewProjectile->SetLifeSpan(FMath::Min(NewProjectile->GetLifeSpan(), 2.f * FMath::Max(0.f, CatchupTickDelta)));
            //NewProjectile->SetLifeSpan(FMath::Min(NewProjectile->GetLifeSpan(), (2.f * FMath::Max(0.f, CatchupTickDelta)) + .1f));
            NewProjectile->SetLifeSpan(FMath::Min(NewProjectile->GetLifeSpan(), 4.f * FMath::Max(0.f, CatchupTickDelta)));
            //NewProjectile->SetLifeSpan(FMath::Min(NewProjectile->GetLifeSpan(), 1.f));
        }
    }

    return FGSProjectileSpawnInfo(
        NewProjectile,
        CatchupTickDelta,
        !bHasAuthority
        );
}

void AGSWeapon::SpawnDelayedFakeProjectile()
{
    AGSPlayerController* OwningPlayer = OwningCharacter
        ? Cast<AGSPlayerController>(OwningCharacter->GetController())
        : NULL;

    if (! OwningPlayer)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Instigator = OwningCharacter;
    SpawnParams.Owner = OwningCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGSUTProjectile* NewProjectile = GetWorld()->SpawnActor<AGSUTProjectile>(
        DelayedProjectile.ProjectileClass,
        DelayedProjectile.SpawnLocation,
        DelayedProjectile.SpawnRotation,
        SpawnParams
        );

    if (NewProjectile)
    {
        float MaxPredictionPing = OwningPlayer->GetMaxPredictionPing();
        float PredictionFudgeFactor = OwningPlayer->GetPredictionFudgeFactor();

        NewProjectile->InitFakeProjectile(OwningPlayer);
        NewProjectile->SetLifeSpan(
            FMath::Min(
                NewProjectile->GetLifeSpan(),
                0.002f * FMath::Max(0.f, MaxPredictionPing+PredictionFudgeFactor)
                )
            );

        //if (NewProjectile->OffsetVisualComponent)
        //{
        //    switch (GetWeaponHand())
        //    {
        //    case EWeaponHand::HAND_Center:
        //        NewProjectile->InitialVisualOffset = NewProjectile->InitialVisualOffset + LowMeshOffset;
        //        NewProjectile->OffsetVisualComponent->RelativeLocation = NewProjectile->InitialVisualOffset;
        //        break;
        //    case EWeaponHand::HAND_Hidden:
        //    {
        //        NewProjectile->InitialVisualOffset = NewProjectile->InitialVisualOffset + VeryLowMeshOffset;
        //        NewProjectile->OffsetVisualComponent->RelativeLocation = NewProjectile->InitialVisualOffset;
        //        break;
        //    }
        //    }
        //}
    }
}

AGSUTProjectile* AGSWeapon::ForwardTickProjectile(
    AGSUTProjectile* InProjectile,
    float InCatchupTickDelta
    )
{
    if (! InProjectile)
    {
        return nullptr;
    }

    if ((InCatchupTickDelta > 0.f) && InProjectile->ProjectileMovement)
    {
        // server ticks projectile to match with
        // when client actually fired
        //
        // TODO: account for CustomTimeDilation?
        if (InProjectile->PrimaryActorTick.IsTickFunctionEnabled())
        {
            InProjectile->TickActor(
                InCatchupTickDelta * InProjectile->CustomTimeDilation,
                LEVELTICK_All,
                InProjectile->PrimaryActorTick
                );
        }

        InProjectile->ProjectileMovement->TickComponent(
            InCatchupTickDelta * InProjectile->CustomTimeDilation,
            LEVELTICK_All,
            NULL
            );

        InProjectile->SetForwardTicked(true);

        if (InProjectile->GetLifeSpan() > 0.f)
        {
            InProjectile->SetLifeSpan(0.1f + FMath::Max(0.01f, InProjectile->GetLifeSpan() - InCatchupTickDelta));
        }
    }
    else
    {
        InProjectile->SetForwardTicked(false);
    }

    return InProjectile;
}

void AGSWeapon::OnRep_PrimaryClipAmmo(int32 OldPrimaryClipAmmo)
{
    OnPrimaryClipAmmoChanged.Broadcast(OldPrimaryClipAmmo, PrimaryClipAmmo);
}

void AGSWeapon::OnRep_MaxPrimaryClipAmmo(int32 OldMaxPrimaryClipAmmo)
{
    OnMaxPrimaryClipAmmoChanged.Broadcast(OldMaxPrimaryClipAmmo, MaxPrimaryClipAmmo);
}

void AGSWeapon::OnRep_SecondaryClipAmmo(int32 OldSecondaryClipAmmo)
{
    OnSecondaryClipAmmoChanged.Broadcast(OldSecondaryClipAmmo, SecondaryClipAmmo);
}

void AGSWeapon::OnRep_MaxSecondaryClipAmmo(int32 OldMaxSecondaryClipAmmo)
{
    OnMaxSecondaryClipAmmoChanged.Broadcast(OldMaxSecondaryClipAmmo, MaxSecondaryClipAmmo);
}

void AGSWeapon::MulticastExecuteGameplayCueLocal_Implementation(
    const FGameplayTag GameplayCueTag,
    const FGameplayCueParameters& GameplayCueParameters,
    bool bSimulatedOnly
    )
{
    if (IsValid(AbilitySystemComponent) &&
        (bSimulatedOnly && (GetLocalRole() == ROLE_SimulatedProxy)))
    {
        AbilitySystemComponent->ExecuteGameplayCueLocal(
            GameplayCueTag,
            GameplayCueParameters
            );
    }
}

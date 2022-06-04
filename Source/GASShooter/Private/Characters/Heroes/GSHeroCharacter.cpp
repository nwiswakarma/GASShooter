// Copyright 2020 Dan Kestranek.

#include "Characters/Heroes/GSHeroCharacter.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"

#include "GASShooterGameModeBase.h"
#include "GSBlueprintFunctionLibrary.h"
#include "AI/GSHeroAIController.h"
#include "Characters/Abilities/GSAbilitySystemComponent.h"
#include "Characters/Abilities/GSAbilitySystemGlobals.h"
#include "Characters/Abilities/AttributeSets/GSAmmoAttributeSet.h"
#include "Characters/Abilities/AttributeSets/GSAttributeSetBase.h"
#include "Player/GSPlayerController.h"
#include "Player/GSPlayerState.h"
#include "UI/GSFloatingStatusBarWidget.h"

#include "Character/ALSCharacterMovementComponent.h"
#include "Character/Animation/ALSCharacterAnimInstance.h"

#include "Weapons/GSWeapon.h"

AGSHeroCharacter::AGSHeroCharacter(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;

    BaseTurnRate = 45.0f;
    BaseLookUpRate = 45.0f;

    bASCInputBound = false;
    bChangedWeaponLocally = false;

    Inventory = FGSHeroInventory();
    ReviveDuration = 4.0f;

    NoWeaponTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Equipped.None"));
    WeaponChangingDelayReplicationTag = FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChangingDelayReplication"));
    WeaponAmmoTypeNoneTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.None"));
    WeaponAbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Weapon"));
    CurrentWeaponTag = NoWeaponTag;

    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionProfileName(FName("NoCollision"));
    GetMesh()->SetCollisionResponseToChannel(COLLISION_INTERACTABLE, ECollisionResponse::ECR_Overlap);
    GetMesh()->bCastHiddenShadow = true;
    GetMesh()->bReceivesDecals = false;

    UIFloatingStatusBarComponent = CreateDefaultSubobject<UWidgetComponent>(FName("UIFloatingStatusBarComponent"));
    UIFloatingStatusBarComponent->SetupAttachment(RootComponent);
    UIFloatingStatusBarComponent->SetRelativeLocation(FVector(0, 0, 120));
    UIFloatingStatusBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
    UIFloatingStatusBarComponent->SetDrawSize(FVector2D(500, 500));

    UIFloatingStatusBarClass = StaticLoadClass(UObject::StaticClass(), nullptr, TEXT("/Game/GASShooter/UI/UI_FloatingStatusBar_Hero.UI_FloatingStatusBar_Hero_C"));

    if (!UIFloatingStatusBarClass)
    {
        UE_LOG(LogTemp, Error, TEXT("%s() Failed to find UIFloatingStatusBarClass. If it was moved, please update the reference location in C++."), *FString(__FUNCTION__));
    }

    AutoPossessAI = EAutoPossessAI::PlacedInWorld;
    AIControllerClass = AGSHeroAIController::StaticClass();

    // Cache tags
    KnockedDownTag = FGameplayTag::RequestGameplayTag("State.KnockedDown");
    InteractingTag = FGameplayTag::RequestGameplayTag("State.Interacting");

    bUseFixedMovementRotation = true;
    bUseAimInput = true;
}

void AGSHeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AGSHeroCharacter, Inventory);

    // Only replicate CurrentWeapon to simulated clients and manually sync CurrentWeeapon with Owner when we're ready.
    // This allows us to predict weapon changing.
    DOREPLIFETIME_CONDITION(AGSHeroCharacter, CurrentWeapon, COND_SimulatedOnly);

    //DOREPLIFETIME_CONDITION(AGSHeroCharacter, AimingRotation, COND_SkipOwner);

    // ALS

    //DOREPLIFETIME(AGSHeroCharacter, TargetRagdollLocation);
    DOREPLIFETIME_CONDITION(AGSHeroCharacter, ReplicatedCurrentAcceleration, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(AGSHeroCharacter, ReplicatedControlRotation, COND_SkipOwner);

    DOREPLIFETIME(AGSHeroCharacter, DesiredGait);
    DOREPLIFETIME_CONDITION(AGSHeroCharacter, DesiredStance, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(AGSHeroCharacter, DesiredRotationMode, COND_SkipOwner);

    DOREPLIFETIME_CONDITION(AGSHeroCharacter, RotationMode, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(AGSHeroCharacter, OverlayState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(AGSHeroCharacter, ViewMode, COND_SkipOwner);
}

// Called to bind functionality to input
void AGSHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis("MoveForward", this, &AGSHeroCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &AGSHeroCharacter::MoveRight);

    PlayerInputComponent->BindAxis("LookUp", this, &AGSHeroCharacter::LookUp);
    PlayerInputComponent->BindAxis("LookUpRate", this, &AGSHeroCharacter::LookUpRate);
    PlayerInputComponent->BindAxis("Turn", this, &AGSHeroCharacter::Turn);
    PlayerInputComponent->BindAxis("TurnRate", this, &AGSHeroCharacter::TurnRate);

    // Bind player input to the AbilitySystemComponent. Also called in OnRep_PlayerState because of a potential race condition.
    BindASCInput();
}

// Server only
void AGSHeroCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    AGSPlayerState* PS = GetPlayerState<AGSPlayerState>();
    if (PS)
    {
        // Set the ASC on the Server. Clients do this in OnRep_PlayerState()
        AbilitySystemComponent = Cast<UGSAbilitySystemComponent>(PS->GetAbilitySystemComponent());

        // AI won't have PlayerControllers so we can init again here just to be sure. No harm in initing twice for heroes that have PlayerControllers.
        PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);

        WeaponChangingDelayReplicationTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(WeaponChangingDelayReplicationTag)
            .AddUObject(this, &AGSHeroCharacter::WeaponChangingDelayReplicationTagChanged);

        // Set the AttributeSetBase for convenience attribute functions
        AttributeSetBase = PS->GetAttributeSetBase();

        AmmoAttributeSet = PS->GetAmmoAttributeSet();

        // If we handle players disconnecting and rejoining in the future, we'll have to change this so that possession from rejoining doesn't reset attributes.
        // For now assume possession = spawn/respawn.
        InitializeAttributes();

        AddStartupEffects();

        AddCharacterAbilities();

        if (AbilitySystemComponent->GetTagCount(DeadTag) > 0)
        {
            // Set Health/Mana/Stamina to their max. This is only necessary for *Respawn*.
            SetHealth(GetMaxHealth());
            SetMana(GetMaxMana());
            SetStamina(GetMaxStamina());
            SetShield(GetMaxShield());
        }

        // Remove Dead tag
        AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(DeadTag));

        InitializeFloatingStatusBar();

        // If player is host on listen server, the floating status bar would have been created for them from BeginPlay before player possession, hide it
        if (IsLocallyControlled() && IsPlayerControlled() && UIFloatingStatusBarComponent && UIFloatingStatusBar)
        {
            UIFloatingStatusBarComponent->SetVisibility(false, true);
        }
    }

    SetupStartupPerspective();
}

UGSFloatingStatusBarWidget* AGSHeroCharacter::GetFloatingStatusBar()
{
    return UIFloatingStatusBar;
}

void AGSHeroCharacter::KnockDown()
{
    if (!HasAuthority())
    {
        return;
    }

    if (IsValid(AbilitySystemComponent))
    {
        AbilitySystemComponent->CancelAllAbilities();

        FGameplayTagContainer EffectTagsToRemove;
        EffectTagsToRemove.AddTag(EffectRemoveOnDeathTag);
        int32 NumEffectsRemoved = AbilitySystemComponent->RemoveActiveEffectsWithTags(EffectTagsToRemove);

        AbilitySystemComponent->ApplyGameplayEffectToSelf(Cast<UGameplayEffect>(KnockDownEffect->GetDefaultObject()), 1.0f, AbilitySystemComponent->MakeEffectContext());
    }

    SetHealth(GetMaxHealth());
    SetShield(0.0f);
}

void AGSHeroCharacter::PlayKnockDownEffects()
{
    // Play it here instead of in the ability to skip extra replication data
    if (DeathMontage)
    {
        PlayAnimMontage(DeathMontage);
    }

    if (AbilitySystemComponent)
    {
        FGameplayCueParameters GCParameters;
        GCParameters.Location = GetActorLocation();
        AbilitySystemComponent->ExecuteGameplayCueLocal(FGameplayTag::RequestGameplayTag("GameplayCue.Hero.KnockedDown"), GCParameters);
    }
}

void AGSHeroCharacter::PlayReviveEffects()
{
    // Play revive particles or sounds here (we don't have any)
    if (AbilitySystemComponent)
    {
        FGameplayCueParameters GCParameters;
        GCParameters.Location = GetActorLocation();
        AbilitySystemComponent->ExecuteGameplayCueLocal(FGameplayTag::RequestGameplayTag("GameplayCue.Hero.Revived"), GCParameters);
    }
}

void AGSHeroCharacter::FinishDying()
{
    // AGSHeroCharacter doesn't follow AGSCharacterBase's pattern of Die->Anim->FinishDying because AGSHeroCharacter can be knocked down
    // to either be revived, bleed out, or finished off by an enemy.

    if (!HasAuthority())
    {
        return;
    }

    RemoveAllWeaponsFromInventory();

    AbilitySystemComponent->RegisterGameplayTagEvent(WeaponChangingDelayReplicationTag).Remove(WeaponChangingDelayReplicationTagChangedDelegateHandle);

    AGASShooterGameModeBase* GM = Cast<AGASShooterGameModeBase>(GetWorld()->GetAuthGameMode());

    if (GM)
    {
        GM->HeroDied(GetController());
    }

    RemoveCharacterAbilities();

    if (IsValid(AbilitySystemComponent))
    {
        AbilitySystemComponent->CancelAllAbilities();

        FGameplayTagContainer EffectTagsToRemove;
        EffectTagsToRemove.AddTag(EffectRemoveOnDeathTag);
        int32 NumEffectsRemoved = AbilitySystemComponent->RemoveActiveEffectsWithTags(EffectTagsToRemove);

        AbilitySystemComponent->ApplyGameplayEffectToSelf(Cast<UGameplayEffect>(DeathEffect->GetDefaultObject()), 1.0f, AbilitySystemComponent->MakeEffectContext());
    }

    OnCharacterDied.Broadcast(this);

    Super::FinishDying();
}

void AGSHeroCharacter::StartWalking()
{
	SetDesiredGait(EALSGait::Walking);
}

void AGSHeroCharacter::StopWalking()
{
	SetDesiredGait(EALSGait::Running);
}

void AGSHeroCharacter::StartSprinting()
{
	SetDesiredGait(EALSGait::Sprinting);
}

void AGSHeroCharacter::StopSprinting()
{
	SetDesiredGait(EALSGait::Running);
}

FVector AGSHeroCharacter::GetWeaponAttachPointLocation() const
{
    return GetMainMesh()->DoesSocketExist(GetWeaponAttachPoint())
        ? GetMainMesh()->GetSocketLocation(GetWeaponAttachPoint())
        : GetActorLocation();
}

bool AGSHeroCharacter::IsUsingFixedMovementRotation() const
{
    return bUseFixedMovementRotation;
}

bool AGSHeroCharacter::IsUsingAimInput() const
{
    return bUseAimInput;
}

AGSWeapon* AGSHeroCharacter::GetCurrentWeapon() const
{
    return CurrentWeapon;
}

bool AGSHeroCharacter::AddWeaponToInventory(AGSWeapon* NewWeapon, bool bEquipWeapon)
{
    if (DoesWeaponExistInInventory(NewWeapon))
    {
        USoundCue* PickupSound = NewWeapon->GetPickupSound();

        if (PickupSound && IsLocallyControlled())
        {
            UGameplayStatics::SpawnSoundAttached(PickupSound, GetRootComponent());
        }

        if (GetLocalRole() < ROLE_Authority)
        {
            return false;
        }

        // Create a dynamic instant Gameplay Effect
        // to give the primary and secondary ammo
        UGameplayEffect* GEAmmo = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("Ammo")));
        GEAmmo->DurationPolicy = EGameplayEffectDurationType::Instant;

        if (NewWeapon->PrimaryAmmoType != WeaponAmmoTypeNoneTag)
        {
            int32 Idx = GEAmmo->Modifiers.Num();
            GEAmmo->Modifiers.SetNum(Idx + 1);

            FGameplayModifierInfo& InfoPrimaryAmmo = GEAmmo->Modifiers[Idx];
            InfoPrimaryAmmo.ModifierMagnitude = FScalableFloat(NewWeapon->GetPrimaryClipAmmo());
            InfoPrimaryAmmo.ModifierOp = EGameplayModOp::Additive;
            InfoPrimaryAmmo.Attribute = UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(NewWeapon->PrimaryAmmoType);
        }

        if (NewWeapon->SecondaryAmmoType != WeaponAmmoTypeNoneTag)
        {
            int32 Idx = GEAmmo->Modifiers.Num();
            GEAmmo->Modifiers.SetNum(Idx + 1);

            FGameplayModifierInfo& InfoSecondaryAmmo = GEAmmo->Modifiers[Idx];
            InfoSecondaryAmmo.ModifierMagnitude = FScalableFloat(NewWeapon->GetSecondaryClipAmmo());
            InfoSecondaryAmmo.ModifierOp = EGameplayModOp::Additive;
            InfoSecondaryAmmo.Attribute = UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(NewWeapon->SecondaryAmmoType);
        }

        if (GEAmmo->Modifiers.Num() > 0)
        {
            AbilitySystemComponent->ApplyGameplayEffectToSelf(GEAmmo, 1.0f, AbilitySystemComponent->MakeEffectContext());
        }

        NewWeapon->Destroy();

        return false;
    }

    if (GetLocalRole() < ROLE_Authority)
    {
        return false;
    }

    Inventory.Weapons.Add(NewWeapon);
    NewWeapon->SetOwningCharacter(this);
    NewWeapon->AddAbilities();

    if (bEquipWeapon)
    {
        EquipWeapon(NewWeapon);
        ClientSyncCurrentWeapon(CurrentWeapon);
    }

    return true;
}

bool AGSHeroCharacter::RemoveWeaponFromInventory(AGSWeapon* WeaponToRemove)
{
    if (DoesWeaponExistInInventory(WeaponToRemove))
    {
        if (WeaponToRemove == CurrentWeapon)
        {
            UnEquipCurrentWeapon();
        }

        Inventory.Weapons.Remove(WeaponToRemove);
        WeaponToRemove->RemoveAbilities();
        WeaponToRemove->SetOwningCharacter(nullptr);
        WeaponToRemove->ResetWeapon();

        // Add parameter to drop weapon?

        return true;
    }

    return false;
}

void AGSHeroCharacter::RemoveAllWeaponsFromInventory()
{
    if (GetLocalRole() < ROLE_Authority)
    {
        return;
    }

    UnEquipCurrentWeapon();

    float radius = 50.0f;
    float NumWeapons = Inventory.Weapons.Num();

    for (int32 i = Inventory.Weapons.Num() - 1; i >= 0; i--)
    {
        AGSWeapon* Weapon = Inventory.Weapons[i];
        RemoveWeaponFromInventory(Weapon);

        // Set the weapon up as a pickup

        float OffsetX = radius * FMath::Cos((i / NumWeapons) * 2.0f * PI);
        float OffsetY = radius * FMath::Sin((i / NumWeapons) * 2.0f * PI);
        Weapon->OnDropped(GetActorLocation() + FVector(OffsetX, OffsetY, 0.0f));
    }
}

void AGSHeroCharacter::EquipWeapon(AGSWeapon* NewWeapon)
{
    if (GetLocalRole() < ROLE_Authority)
    {
        ServerEquipWeapon(NewWeapon);
        SetCurrentWeapon(NewWeapon, CurrentWeapon);
        bChangedWeaponLocally = true;
    }
    else
    {
        SetCurrentWeapon(NewWeapon, CurrentWeapon);
    }
}

void AGSHeroCharacter::ServerEquipWeapon_Implementation(AGSWeapon* NewWeapon)
{
    EquipWeapon(NewWeapon);
}

bool AGSHeroCharacter::ServerEquipWeapon_Validate(AGSWeapon* NewWeapon)
{
    return true;
}

void AGSHeroCharacter::NextWeapon()
{
    if (Inventory.Weapons.Num() < 2)
    {
        return;
    }

    int32 CurrentWeaponIndex = Inventory.Weapons.Find(CurrentWeapon);
    UnEquipCurrentWeapon();

    if (CurrentWeaponIndex == INDEX_NONE)
    {
        EquipWeapon(Inventory.Weapons[0]);
    }
    else
    {
        EquipWeapon(Inventory.Weapons[(CurrentWeaponIndex + 1) % Inventory.Weapons.Num()]);
    }
}

void AGSHeroCharacter::PreviousWeapon()
{
    if (Inventory.Weapons.Num() < 2)
    {
        return;
    }

    int32 CurrentWeaponIndex = Inventory.Weapons.Find(CurrentWeapon);

    UnEquipCurrentWeapon();

    if (CurrentWeaponIndex == INDEX_NONE)
    {
        EquipWeapon(Inventory.Weapons[0]);
    }
    else
    {
        int32 IndexOfPrevWeapon = FMath::Abs(CurrentWeaponIndex - 1 + Inventory.Weapons.Num()) % Inventory.Weapons.Num();
        EquipWeapon(Inventory.Weapons[IndexOfPrevWeapon]);
    }
}

void AGSHeroCharacter::UnEquipWeapon()
{
    UnEquipCurrentWeapon();
    EquipWeapon(nullptr);
}

FName AGSHeroCharacter::GetWeaponAttachPoint() const
{
    return WeaponAttachPoint;
}

int32 AGSHeroCharacter::GetPrimaryClipAmmo() const
{
    if (CurrentWeapon)
    {
        return CurrentWeapon->GetPrimaryClipAmmo();
    }

    return 0;
}

int32 AGSHeroCharacter::GetMaxPrimaryClipAmmo() const
{
    if (CurrentWeapon)
    {
        return CurrentWeapon->GetMaxPrimaryClipAmmo();
    }

    return 0;
}

int32 AGSHeroCharacter::GetPrimaryReserveAmmo() const
{
    if (CurrentWeapon && AmmoAttributeSet)
    {
        FGameplayAttribute Attribute = AmmoAttributeSet->GetReserveAmmoAttributeFromTag(CurrentWeapon->PrimaryAmmoType);
        if (Attribute.IsValid())
        {
            return AbilitySystemComponent->GetNumericAttribute(Attribute);
        }
    }

    return 0;
}

int32 AGSHeroCharacter::GetSecondaryClipAmmo() const
{
    if (CurrentWeapon)
    {
        return CurrentWeapon->GetSecondaryClipAmmo();
    }

    return 0;
}

int32 AGSHeroCharacter::GetMaxSecondaryClipAmmo() const
{
    if (CurrentWeapon)
    {
        return CurrentWeapon->GetMaxSecondaryClipAmmo();
    }

    return 0;
}

int32 AGSHeroCharacter::GetSecondaryReserveAmmo() const
{
    if (CurrentWeapon)
    {
        FGameplayAttribute Attribute = AmmoAttributeSet->GetReserveAmmoAttributeFromTag(CurrentWeapon->SecondaryAmmoType);
        if (Attribute.IsValid())
        {
            return AbilitySystemComponent->GetNumericAttribute(Attribute);
        }
    }

    return 0;
}

int32 AGSHeroCharacter::GetNumWeapons() const
{
    return Inventory.Weapons.Num();
}

bool AGSHeroCharacter::IsAvailableForInteraction_Implementation(UPrimitiveComponent* InteractionComponent) const
{
    // Hero is available to be revived if knocked down and is not already being revived.
    // If you want multiple heroes reviving someone to speed it up, you would need to change GA_Interact
    // (outside the scope of this sample).
    if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag)
        && !AbilitySystemComponent->HasMatchingGameplayTag(InteractingTag))
    {
        return true;
    }
    
    return IGSInteractable::IsAvailableForInteraction_Implementation(InteractionComponent);
}

float AGSHeroCharacter::GetInteractionDuration_Implementation(UPrimitiveComponent* InteractionComponent) const
{
    if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag))
    {
        return ReviveDuration;
    }

    return IGSInteractable::GetInteractionDuration_Implementation(InteractionComponent);
}

void AGSHeroCharacter::PreInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* InteractionComponent)
{
    if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag) && HasAuthority())
    {
        AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(FGameplayTag::RequestGameplayTag("Ability.Revive")));
    }
}

void AGSHeroCharacter::PostInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* InteractionComponent)
{
    if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag) && HasAuthority())
    {
        AbilitySystemComponent->ApplyGameplayEffectToSelf(Cast<UGameplayEffect>(ReviveEffect->GetDefaultObject()), 1.0f, AbilitySystemComponent->MakeEffectContext());
    }
}

void AGSHeroCharacter::GetPreInteractSyncType_Implementation(bool& bShouldSync, EAbilityTaskNetSyncType& Type, UPrimitiveComponent* InteractionComponent) const
{
    if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag))
    {
        bShouldSync = true;
        Type = EAbilityTaskNetSyncType::OnlyClientWait;
        return;
    }

    IGSInteractable::GetPreInteractSyncType_Implementation(bShouldSync, Type, InteractionComponent);
}

void AGSHeroCharacter::CancelInteraction_Implementation(UPrimitiveComponent* InteractionComponent)
{
    if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag) && HasAuthority())
    {
        FGameplayTagContainer CancelTags(FGameplayTag::RequestGameplayTag("Ability.Revive"));
        AbilitySystemComponent->CancelAbilities(&CancelTags);
    }
}

FSimpleMulticastDelegate* AGSHeroCharacter::GetTargetCancelInteractionDelegate(UPrimitiveComponent* InteractionComponent)
{
    return &InteractionCanceledDelegate;
}

/**
* On the Server, Possession happens before BeginPlay.
* On the Client, BeginPlay happens before Possession.
* So we can't use BeginPlay to do anything with the AbilitySystemComponent because we don't have it until the PlayerState replicates from possession.
*/
void AGSHeroCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Only needed for Heroes placed in world and when the player is the Server.
    // On respawn, they are set up in PossessedBy.
    // When the player a client, the floating status bars are all set up in OnRep_PlayerState.
    InitializeFloatingStatusBar();

    // CurrentWeapon is replicated only to Simulated clients so sync the current weapon manually
    if (GetLocalRole() == ROLE_AutonomousProxy)
    {
        ServerSyncCurrentWeapon();
    }

    /** ALS BeginPlay() */

    // If we're in networked game, disable curved movement
    bEnableNetworkOptimizations = !IsNetMode(NM_Standalone);

    // Make sure the mesh and animbp update after the CharacterBP to ensure it gets the most recent values.
    GetMainMesh()->AddTickPrerequisiteActor(this);

    // Set the Movement Model
    SetMovementModel();

    // ALS settings initialization

    // Once, force set variables in anim bp. This ensures anim instance & character starts synchronized
    FALSAnimCharacterInformation& AnimData = MainAnimInstance->GetCharacterInformationMutable();
    MainAnimInstance->Gait = DesiredGait;
    MainAnimInstance->Stance = DesiredStance;
    MainAnimInstance->RotationMode = DesiredRotationMode;
    AnimData.ViewMode = ViewMode;
    MainAnimInstance->OverlayState = OverlayState;
    AnimData.PrevMovementState = PrevMovementState;
    MainAnimInstance->MovementState = MovementState;

    // Update states to use the initial desired values.
    SetGait(DesiredGait);
    SetStance(DesiredStance);
    SetRotationMode(DesiredRotationMode);
    SetViewMode(ViewMode);
    SetOverlayState(OverlayState);

    if (Stance == EALSStance::Standing)
    {
        UnCrouch();
    }
    else
    if (Stance == EALSStance::Crouching)
    {
        Crouch();
    }

    // Set default rotation values.
    TargetRotation = GetActorRotation();
    LastVelocityRotation = TargetRotation;
    LastMovementInputRotation = TargetRotation;

    if (GetLocalRole() == ROLE_SimulatedProxy)
    {
        MainAnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    }
}

void AGSHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Cancel being revived if killed
    //InteractionCanceledDelegate.Broadcast();
    Execute_InteractableCancelInteraction(this, GetMainMesh());

    // Clear CurrentWeaponTag on the ASC.
    //
    // This happens naturally in UnEquipCurrentWeapon()
    // but that is only called on the server from hero death
    // (the OnRep_CurrentWeapon() would have handled it
    // on the client but that is never called due to
    // the hero being marked pending destroy).
    //
    // This makes sure the client has it cleared.
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->RemoveLooseGameplayTag(CurrentWeaponTag);
        CurrentWeaponTag = NoWeaponTag;
        AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
    }

    Super::EndPlay(EndPlayReason);
}

void AGSHeroCharacter::PreInitializeComponents()
{
    Super::PreInitializeComponents();

    MainAnimInstance = Cast<UALSCharacterAnimInstance>(GetMainMesh()->GetAnimInstance());

    if (! MainAnimInstance)
    {
        // Animation instance should be assigned if we're not in editor preview
        checkf(
            GetWorld()->WorldType == EWorldType::EditorPreview,
            TEXT("%s doesn't have a valid animation instance assigned. That's not allowed"), *GetName()
            );
    }
}

void AGSHeroCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    GSMovementComponent = Cast<UGSCharacterMovementComponent>(GetMovementComponent());

    AimingRotation = GetActorRotation();
    ProjectedCameraViewLocation = GetActorLocation();

    GetWorldTimerManager().SetTimerForNextTick(this, &AGSHeroCharacter::SpawnDefaultInventory);
}

void AGSHeroCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Set required values
    SetEssentialValues(DeltaTime);

    if (MovementState == EALSMovementState::Grounded)
    {
        UpdateCharacterMovement();
        UpdateGroundedRotation(DeltaTime);
    }
    else
    if (MovementState == EALSMovementState::InAir)
    {
        UpdateInAirRotation(DeltaTime);
    }
    //else
    //if (MovementState == EALSMovementState::Ragdoll)
    //{
    //    RagdollUpdate(DeltaTime);
    //}

    // Update rest of animation data character information
    FALSAnimCharacterInformation& AnimData(MainAnimInstance->GetCharacterInformationMutable());
    AnimData.Velocity = GetCharacterMovement()->Velocity;
    AnimData.MovementInput = GetMovementInput();
    AnimData.AimingRotation = GetAimingRotation();
    AnimData.CharacterActorRotation = GetActorRotation();

    // Cache values
    PreviousVelocity = GetVelocity();
    PreviousAimYaw = AimingRotation.Yaw;
}

void AGSHeroCharacter::LookUp(float Value)
{
    if (IsAlive() && !bUseAimInput)
    {
        AddControllerPitchInput(Value * LookUpDownRate);
    }
}

void AGSHeroCharacter::LookUpRate(float Value)
{
    if (IsAlive() && !bUseAimInput)
    {
        AddControllerPitchInput(Value * BaseLookUpRate * GetWorld()->DeltaTimeSeconds);
    }
}

void AGSHeroCharacter::Turn(float Value)
{
    if (IsAlive() && !bUseAimInput)
    {
        AddControllerYawInput(Value * LookLeftRightRate);
    }
}

void AGSHeroCharacter::TurnRate(float Value)
{
    if (IsAlive() && !bUseAimInput)
    {
        AddControllerYawInput(Value * BaseTurnRate * GetWorld()->DeltaTimeSeconds);
    }
}

void AGSHeroCharacter::MoveForward(float Value)
{
    if (IsAlive())
    {
        if (bUseFixedMovementRotation)
        {
            AddMovementInput(UKismetMathLibrary::GetForwardVector(MovementFixedRotation), Value);
        }
        else
        {
            AddMovementInput(UKismetMathLibrary::GetForwardVector(FRotator(0, GetControlRotation().Yaw, 0)), Value);
        }
    }
}

void AGSHeroCharacter::MoveRight(float Value)
{
    if (IsAlive())
    {
        if (bUseFixedMovementRotation)
        {
            AddMovementInput(UKismetMathLibrary::GetRightVector(MovementFixedRotation), Value);
        }
        else
        {
            AddMovementInput(UKismetMathLibrary::GetRightVector(FRotator(0, GetControlRotation().Yaw, 0)), Value);
        }
    }
}

USkeletalMeshComponent* AGSHeroCharacter::GetMainMesh() const
{
    return GetMesh();
}

FVector AGSHeroCharacter::GetProjectionAnchorOffset() const
{
    return ProjectionAnchorOffset;
}

FRotator AGSHeroCharacter::GetViewRotation() const
{
    if (bUseAimInput)
    {
        return GetAimingRotation();
    }
    else
    {
        return Super::GetViewRotation();
    }
}

FRotator AGSHeroCharacter::GetAimingRotation() const
{
    return AimingRotation;
}

void AGSHeroCharacter::SetAimingRotation(FRotator NewAimRotation)
{
    if (GetLocalRole() < ROLE_Authority)
    {
        Server_SetAimingRotation(NewAimRotation);
        AimingRotation = NewAimRotation;
    }
    else
    {
        AimingRotation = NewAimRotation;
    }
}

void AGSHeroCharacter::Server_SetAimingRotation_Implementation(FRotator NewAimRotation)
{
    SetAimingRotation(NewAimRotation);
}

void AGSHeroCharacter::BindASCInput()
{
    if (!bASCInputBound && IsValid(AbilitySystemComponent) && IsValid(InputComponent))
    {
        AbilitySystemComponent->BindAbilityActivationToInputComponent(InputComponent, FGameplayAbilityInputBinds(FString("ConfirmTarget"),
            FString("CancelTarget"), FString("EGSAbilityInputID"), static_cast<int32>(EGSAbilityInputID::Confirm), static_cast<int32>(EGSAbilityInputID::Cancel)));

        bASCInputBound = true;
    }
}

void AGSHeroCharacter::InitializeFloatingStatusBar()
{
    // Only create once
    if (UIFloatingStatusBar || !IsValid(AbilitySystemComponent))
    {
        return;
    }

    // Don't create for locally controlled player. We could add a game setting to toggle this later.
    if (IsPlayerControlled() && IsLocallyControlled())
    {
        return;
    }

    // Need a valid PlayerState
    if (!GetPlayerState())
    {
        return;
    }

    // Setup UI for Locally Owned Players only, not AI or the server's copy of the PlayerControllers
    AGSPlayerController* PC = Cast<AGSPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
    if (PC && PC->IsLocalPlayerController())
    {
        if (UIFloatingStatusBarClass)
        {
            UIFloatingStatusBar = CreateWidget<UGSFloatingStatusBarWidget>(PC, UIFloatingStatusBarClass);
            if (UIFloatingStatusBar && UIFloatingStatusBarComponent)
            {
                UIFloatingStatusBarComponent->SetWidget(UIFloatingStatusBar);

                // Setup the floating status bar
                UIFloatingStatusBar->SetHealthPercentage(GetHealth() / GetMaxHealth());
                UIFloatingStatusBar->SetManaPercentage(GetMana() / GetMaxMana());
                UIFloatingStatusBar->SetShieldPercentage(GetShield() / GetMaxShield());
                UIFloatingStatusBar->OwningCharacter = this;
                UIFloatingStatusBar->SetCharacterName(CharacterName);
            }
        }
    }
}

// Client only
void AGSHeroCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    AGSPlayerState* PS = GetPlayerState<AGSPlayerState>();
    if (PS)
    {
        // Set the ASC for clients. Server does this in PossessedBy.
        AbilitySystemComponent = Cast<UGSAbilitySystemComponent>(PS->GetAbilitySystemComponent());

        // Init ASC Actor Info for clients. Server will init its ASC when it possesses a new Actor.
        AbilitySystemComponent->InitAbilityActorInfo(PS, this);

        // Bind player input to the AbilitySystemComponent. Also called in SetupPlayerInputComponent because of a potential race condition.
        BindASCInput();

        AbilitySystemComponent->AbilityFailedCallbacks.AddUObject(this, &AGSHeroCharacter::OnAbilityActivationFailed);

        // Set the AttributeSetBase for convenience attribute functions
        AttributeSetBase = PS->GetAttributeSetBase();
        
        AmmoAttributeSet = PS->GetAmmoAttributeSet();

        // If we handle players disconnecting and rejoining in the future, we'll have to change this so that posession from rejoining doesn't reset attributes.
        // For now assume possession = spawn/respawn.
        InitializeAttributes();

        AGSPlayerController* PC = Cast<AGSPlayerController>(GetController());

        if (PC)
        {
            PC->CreateHUD();
        }
        
        if (CurrentWeapon)
        {
            // If current weapon repped before PlayerState, set tag on ASC
            AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
            // Update owning character and ASC just in case it repped before PlayerState
            CurrentWeapon->SetOwningCharacter(this);

            if (!PrimaryReserveAmmoChangedDelegateHandle.IsValid())
            {
                PrimaryReserveAmmoChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->PrimaryAmmoType)).AddUObject(this, &AGSHeroCharacter::CurrentWeaponPrimaryReserveAmmoChanged);
            }
            if (!SecondaryReserveAmmoChangedDelegateHandle.IsValid())
            {
                SecondaryReserveAmmoChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->SecondaryAmmoType)).AddUObject(this, &AGSHeroCharacter::CurrentWeaponSecondaryReserveAmmoChanged);
            }
        }

        if (AbilitySystemComponent->GetTagCount(DeadTag) > 0)
        {
            // Set Health/Mana/Stamina/Shield to their max. This is only for *Respawn*. It will be set (replicated) by the
            // Server, but we call it here just to be a little more responsive.
            SetHealth(GetMaxHealth());
            SetMana(GetMaxMana());
            SetStamina(GetMaxStamina());
            SetShield(GetMaxShield());
        }

        // Simulated on proxies don't have their PlayerStates yet when BeginPlay is called so we call it again here
        InitializeFloatingStatusBar();
    }
}

void AGSHeroCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();

    SetupStartupPerspective();
}

void AGSHeroCharacter::SpawnDefaultInventory()
{
    if (GetLocalRole() < ROLE_Authority)
    {
        return;
    }

    int32 NumWeaponClasses = DefaultInventoryWeaponClasses.Num();
    for (int32 i = 0; i < NumWeaponClasses; i++)
    {
        if (!DefaultInventoryWeaponClasses[i])
        {
            // An empty item was added to the Array in blueprint
            continue;
        }

        AGSWeapon* NewWeapon = GetWorld()->SpawnActorDeferred<AGSWeapon>(DefaultInventoryWeaponClasses[i],
            FTransform::Identity, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        NewWeapon->bSpawnWithCollision = false;
        NewWeapon->FinishSpawning(FTransform::Identity);

        bool bEquipFirstWeapon = i == 0;
        AddWeaponToInventory(NewWeapon, bEquipFirstWeapon);
    }
}

void AGSHeroCharacter::SetupStartupPerspective()
{
    APlayerController* PC = Cast<APlayerController>(GetController());

    // Assign view target if player controlled and has valid camera
    if (PC && PC->IsLocalController() && IsValid(GetCamera()))
    {
        GetCamera()->Activate();
        PC->SetViewTarget(this);
    }
}

bool AGSHeroCharacter::DoesWeaponExistInInventory(AGSWeapon* InWeapon)
{
    //UE_LOG(LogTemp, Log, TEXT("%s InWeapon class %s"), *FString(__FUNCTION__), *InWeapon->GetClass()->GetName());

    for (AGSWeapon* Weapon : Inventory.Weapons)
    {
        if (Weapon && InWeapon && Weapon->GetClass() == InWeapon->GetClass())
        {
            return true;
        }
    }

    return false;
}

void AGSHeroCharacter::SetCurrentWeapon(AGSWeapon* NewWeapon, AGSWeapon* LastWeapon)
{
    if (NewWeapon == LastWeapon)
    {
        return;
    }

    // Cancel active weapon abilities
    if (AbilitySystemComponent)
    {
        FGameplayTagContainer AbilityTagsToCancel = FGameplayTagContainer(WeaponAbilityTag);
        AbilitySystemComponent->CancelAbilities(&AbilityTagsToCancel);
    }

    UnEquipWeapon(LastWeapon);

    if (NewWeapon)
    {
        if (AbilitySystemComponent)
        {
            // Clear out potential NoWeaponTag
            AbilitySystemComponent->RemoveLooseGameplayTag(CurrentWeaponTag);
        }

        // Weapons coming from OnRep_CurrentWeapon won't have the owner set
        CurrentWeapon = NewWeapon;
        CurrentWeapon->SetOwningCharacter(this);
        CurrentWeapon->Equip();
        CurrentWeaponTag = CurrentWeapon->WeaponTag;

        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
        }

        AGSPlayerController* PC = GetController<AGSPlayerController>();
        if (PC && PC->IsLocalController())
        {
            PC->SetEquippedWeaponPrimaryIconFromSprite(CurrentWeapon->PrimaryIcon);
            PC->SetEquippedWeaponStatusText(CurrentWeapon->StatusText);
            PC->SetPrimaryClipAmmo(CurrentWeapon->GetPrimaryClipAmmo());
            PC->SetPrimaryReserveAmmo(GetPrimaryReserveAmmo());
            PC->SetHUDReticle(CurrentWeapon->GetPrimaryHUDReticleClass());
        }

        NewWeapon->OnPrimaryClipAmmoChanged.AddDynamic(this, &AGSHeroCharacter::CurrentWeaponPrimaryClipAmmoChanged);
        NewWeapon->OnSecondaryClipAmmoChanged.AddDynamic(this, &AGSHeroCharacter::CurrentWeaponSecondaryClipAmmoChanged);
        
        if (AbilitySystemComponent)
        {
            PrimaryReserveAmmoChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->PrimaryAmmoType)).AddUObject(this, &AGSHeroCharacter::CurrentWeaponPrimaryReserveAmmoChanged);
            SecondaryReserveAmmoChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->SecondaryAmmoType)).AddUObject(this, &AGSHeroCharacter::CurrentWeaponSecondaryReserveAmmoChanged);
        }

        UAnimMontage* Equip3PMontage = CurrentWeapon->GetEquip3PMontage();
        if (Equip3PMontage && GetMainMesh())
        {
            GetMainMesh()->GetAnimInstance()->Montage_Play(Equip3PMontage);
        }
    }
    else
    {
        // This will clear HUD, tags etc
        UnEquipCurrentWeapon();
    }

    OnCurrentWeaponChanged.Broadcast(LastWeapon, NewWeapon);
}

void AGSHeroCharacter::UnEquipWeapon(AGSWeapon* WeaponToUnEquip)
{
    //TODO this will run into issues when calling UnEquipWeapon explicitly and the WeaponToUnEquip == CurrentWeapon

    if (WeaponToUnEquip)
    {
        WeaponToUnEquip->OnPrimaryClipAmmoChanged.RemoveDynamic(this, &AGSHeroCharacter::CurrentWeaponPrimaryClipAmmoChanged);
        WeaponToUnEquip->OnSecondaryClipAmmoChanged.RemoveDynamic(this, &AGSHeroCharacter::CurrentWeaponSecondaryClipAmmoChanged);

        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(WeaponToUnEquip->PrimaryAmmoType)).Remove(PrimaryReserveAmmoChangedDelegateHandle);
            AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(WeaponToUnEquip->SecondaryAmmoType)).Remove(SecondaryReserveAmmoChangedDelegateHandle);
        }
        
        WeaponToUnEquip->UnEquip();
    }
}

void AGSHeroCharacter::UnEquipCurrentWeapon()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->RemoveLooseGameplayTag(CurrentWeaponTag);
        CurrentWeaponTag = NoWeaponTag;
        AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
    }

    UnEquipWeapon(CurrentWeapon);
    CurrentWeapon = nullptr;

    AGSPlayerController* PC = GetController<AGSPlayerController>();
    if (PC && PC->IsLocalController())
    {
        PC->SetEquippedWeaponPrimaryIconFromSprite(nullptr);
        PC->SetEquippedWeaponStatusText(FText());
        PC->SetPrimaryClipAmmo(0);
        PC->SetPrimaryReserveAmmo(0);
        PC->SetHUDReticle(nullptr);
    }
}

void AGSHeroCharacter::CurrentWeaponPrimaryClipAmmoChanged(int32 OldPrimaryClipAmmo, int32 NewPrimaryClipAmmo)
{
    AGSPlayerController* PC = GetController<AGSPlayerController>();

    if (PC && PC->IsLocalController())
    {
        PC->SetPrimaryClipAmmo(NewPrimaryClipAmmo);
    }

    OnCurrentWeaponPrimaryClipAmmoChanged.Broadcast(OldPrimaryClipAmmo, NewPrimaryClipAmmo);
}

void AGSHeroCharacter::CurrentWeaponSecondaryClipAmmoChanged(int32 OldSecondaryClipAmmo, int32 NewSecondaryClipAmmo)
{
    AGSPlayerController* PC = GetController<AGSPlayerController>();
    if (PC && PC->IsLocalController())
    {
        PC->SetSecondaryClipAmmo(NewSecondaryClipAmmo);
    }
}

void AGSHeroCharacter::CurrentWeaponPrimaryReserveAmmoChanged(const FOnAttributeChangeData& Data)
{
    AGSPlayerController* PC = GetController<AGSPlayerController>();
    if (PC && PC->IsLocalController())
    {
        PC->SetPrimaryReserveAmmo(Data.NewValue);
    }
}

void AGSHeroCharacter::CurrentWeaponSecondaryReserveAmmoChanged(const FOnAttributeChangeData& Data)
{
    AGSPlayerController* PC = GetController<AGSPlayerController>();
    if (PC && PC->IsLocalController())
    {
        PC->SetSecondaryReserveAmmo(Data.NewValue);
    }
}

void AGSHeroCharacter::WeaponChangingDelayReplicationTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    if (CallbackTag == WeaponChangingDelayReplicationTag)
    {
        if (NewCount < 1)
        {
            // We only replicate the current weapon to simulated proxies so manually sync it when the weapon changing delay replication
            // tag is removed. We keep the weapon changing tag on for ~1s after the equip montage to allow for activating changing weapon
            // again without the server trying to clobber the next locally predicted weapon.
            ClientSyncCurrentWeapon(CurrentWeapon);
        }
    }
}

void AGSHeroCharacter::OnRep_CurrentWeapon(AGSWeapon* LastWeapon)
{
    bChangedWeaponLocally = false;
    SetCurrentWeapon(CurrentWeapon, LastWeapon);
}

void AGSHeroCharacter::OnRep_Inventory()
{
    if (GetLocalRole() == ROLE_AutonomousProxy && Inventory.Weapons.Num() > 0 && !CurrentWeapon)
    {
        // Since we don't replicate the CurrentWeapon to the owning client, this is a way to ask the Server to sync
        // the CurrentWeapon after it's been spawned via replication from the Server.
        // The weapon spawning is replicated but the variable CurrentWeapon is not on the owning client.
        ServerSyncCurrentWeapon();
    }
}

void AGSHeroCharacter::OnAbilityActivationFailed(const UGameplayAbility* FailedAbility, const FGameplayTagContainer& FailTags)
{
    if (FailedAbility && FailedAbility->AbilityTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChanging"))))
    {
        if (bChangedWeaponLocally)
        {
            // Ask the Server to resync the CurrentWeapon that we predictively changed
            UE_LOG(LogTemp, Warning, TEXT("%s Weapon Changing ability activation failed. Syncing CurrentWeapon. %s. %s"), *FString(__FUNCTION__),
                *UGSBlueprintFunctionLibrary::GetPlayerEditorWindowRole(GetWorld()), *FailTags.ToString());

            ServerSyncCurrentWeapon();
        }
    }
}

void AGSHeroCharacter::ServerSyncCurrentWeapon_Implementation()
{
    ClientSyncCurrentWeapon(CurrentWeapon);
}

bool AGSHeroCharacter::ServerSyncCurrentWeapon_Validate()
{
    return true;
}

void AGSHeroCharacter::ClientSyncCurrentWeapon_Implementation(AGSWeapon* InWeapon)
{
    AGSWeapon* LastWeapon = CurrentWeapon;
    CurrentWeapon = InWeapon;
    OnRep_CurrentWeapon(LastWeapon);
}

bool AGSHeroCharacter::ClientSyncCurrentWeapon_Validate(AGSWeapon* InWeapon)
{
    return true;
}

/** ALS */

// Public

/** ALS - Character States */

void AGSHeroCharacter::SetAcceleration(const FVector& NewAcceleration)
{
    Acceleration = (NewAcceleration != FVector::ZeroVector || IsLocallyControlled())
                       ? NewAcceleration
                       : Acceleration / 2;
    MainAnimInstance->GetCharacterInformationMutable().Acceleration = Acceleration;
}

void AGSHeroCharacter::SetIsMoving(bool bNewIsMoving)
{
    bIsMoving = bNewIsMoving;
    MainAnimInstance->GetCharacterInformationMutable().bIsMoving = bIsMoving;
}

void AGSHeroCharacter::SetMovementInputAmount(float NewMovementInputAmount)
{
    MovementInputAmount = NewMovementInputAmount;
    MainAnimInstance->GetCharacterInformationMutable().MovementInputAmount = MovementInputAmount;
}

void AGSHeroCharacter::SetSpeed(float NewSpeed)
{
    Speed = NewSpeed;
    MainAnimInstance->GetCharacterInformationMutable().Speed = Speed;
}

void AGSHeroCharacter::SetAimYawRate(float NewAimYawRate)
{
    AimYawRate = NewAimYawRate;
    MainAnimInstance->GetCharacterInformationMutable().AimYawRate = AimYawRate;
}

void AGSHeroCharacter::GetControlForwardRightVector(FVector& Forward, FVector& Right) const
{
    const FRotator ControlRot(0.0f, AimingRotation.Yaw, 0.0f);
    Forward = GetInputAxisValue("MoveForward/Backwards") * UKismetMathLibrary::GetForwardVector(ControlRot);
    Right = GetInputAxisValue("MoveRight/Left") * UKismetMathLibrary::GetRightVector(ControlRot);
}

/** ALS - Movement System */

void AGSHeroCharacter::SetHasMovementInput(bool bNewHasMovementInput)
{
    bHasMovementInput = bNewHasMovementInput;
    MainAnimInstance->GetCharacterInformationMutable().bHasMovementInput = bHasMovementInput;
}

FALSMovementSettings AGSHeroCharacter::GetTargetMovementSettings() const
{
    if (RotationMode == EALSRotationMode::VelocityDirection)
    {
        if (Stance == EALSStance::Standing)
        {
            return MovementData.VelocityDirection.Standing;
        }
        if (Stance == EALSStance::Crouching)
        {
            return MovementData.VelocityDirection.Crouching;
        }
    }
    else if (RotationMode == EALSRotationMode::LookingDirection)
    {
        if (Stance == EALSStance::Standing)
        {
            return MovementData.LookingDirection.Standing;
        }
        if (Stance == EALSStance::Crouching)
        {
            return MovementData.LookingDirection.Crouching;
        }
    }
    else if (RotationMode == EALSRotationMode::Aiming)
    {
        if (Stance == EALSStance::Standing)
        {
            return MovementData.Aiming.Standing;
        }
        if (Stance == EALSStance::Crouching)
        {
            return MovementData.Aiming.Crouching;
        }
    }

    // Default to velocity dir standing
    return MovementData.VelocityDirection.Standing;
}

EALSGait AGSHeroCharacter::GetAllowedGait() const
{
    // CURRENTLY BYPASSED
    //
    // Checks are done with gameplay ability system

    return DesiredGait;
}

EALSGait AGSHeroCharacter::GetActualGait(EALSGait AllowedGait) const
{
    // Get actual gait for the specified gait with regards to character speed.
    //
    // This is calculated by the actual movement of the character,
    // and so it can be different from the desired gait or allowed gait.
    //
    // For instance, if the Allowed Gait becomes walking,
    // the Actual gait will still be running until the character
    // decelerates to the walking speed.

    const float WalkSpeed = GSMovementComponent->CurrentMovementSettings.WalkSpeed;
    const float RunSpeed = GSMovementComponent->CurrentMovementSettings.RunSpeed;

    // Character speed faster than run speed
    if (Speed > RunSpeed + 10.0f)
    {
        // Sprint gait requested, return sprint gait
        if (AllowedGait == EALSGait::Sprinting)
        {
            return EALSGait::Sprinting;
        }

        // Otherwise, return run gait
        return EALSGait::Running;
    }

    // Character speed faster than walk speed, return run gait
    if (Speed > WalkSpeed + 10.0f)
    {
        return EALSGait::Running;
    }

    // Otherwise, return walk gait
    return EALSGait::Walking;
}

bool AGSHeroCharacter::CanSprint() const
{
    // Determine if the character is currently able to sprint
    // based on the Rotation mode and current acceleration (input) rotation.
    //
    // If the character is in the Looking Rotation mode,
    // only allow sprinting if there is full movement input
    // and it is faced forward relative to the camera + or - 50 degrees.

    if (!bHasMovementInput || RotationMode == EALSRotationMode::Aiming)
    {
        return false;
    }

    const bool bValidInputAmount = MovementInputAmount > 0.9f;

    if (RotationMode == EALSRotationMode::VelocityDirection)
    {
        return bValidInputAmount;
    }

    if (RotationMode == EALSRotationMode::LookingDirection)
    {
        const FRotator AccRot = ReplicatedCurrentAcceleration.ToOrientationRotator();
        FRotator Delta = AccRot - AimingRotation;
        Delta.Normalize();

        return bValidInputAmount && FMath::Abs(Delta.Yaw) < 50.0f;
    }

    return false;
}

// Protected

/** ALS - Character States */

void AGSHeroCharacter::SetMovementState(const EALSMovementState NewState)
{
    if (MovementState != NewState)
    {
        PrevMovementState = MovementState;
        MovementState = NewState;
        FALSAnimCharacterInformation& AnimData = MainAnimInstance->GetCharacterInformationMutable();
        AnimData.PrevMovementState = PrevMovementState;
        MainAnimInstance->MovementState = MovementState;
        OnMovementStateChanged(PrevMovementState);
    }
}

void AGSHeroCharacter::SetMovementAction(const EALSMovementAction NewAction)
{
    if (MovementAction != NewAction)
    {
        const EALSMovementAction Prev = MovementAction;
        MovementAction = NewAction;
        MainAnimInstance->MovementAction = MovementAction;
        OnMovementActionChanged(Prev);
    }
}

void AGSHeroCharacter::SetStance(const EALSStance NewStance)
{
    if (Stance != NewStance)
    {
        const EALSStance Prev = Stance;
        Stance = NewStance;
        OnStanceChanged(Prev);
    }
}

void AGSHeroCharacter::SetGait(const EALSGait NewGait)
{
    if (Gait != NewGait)
    {
        const EALSGait Prev = Gait;
        Gait = NewGait;
        OnGaitChanged(Prev);
    }
}


void AGSHeroCharacter::SetDesiredStance(EALSStance NewStance)
{
    DesiredStance = NewStance;
    if (GetLocalRole() == ROLE_AutonomousProxy)
    {
        Server_SetDesiredStance(NewStance);
    }
}

void AGSHeroCharacter::Server_SetDesiredStance_Implementation(EALSStance NewStance)
{
    SetDesiredStance(NewStance);
}

void AGSHeroCharacter::SetDesiredGait(const EALSGait NewGait)
{
    DesiredGait = NewGait;
    if (GetLocalRole() == ROLE_AutonomousProxy)
    {
        Server_SetDesiredGait(NewGait);
    }
}

void AGSHeroCharacter::Server_SetDesiredGait_Implementation(EALSGait NewGait)
{
    SetDesiredGait(NewGait);
}

void AGSHeroCharacter::SetDesiredRotationMode(EALSRotationMode NewRotMode)
{
    DesiredRotationMode = NewRotMode;
    if (GetLocalRole() == ROLE_AutonomousProxy)
    {
        Server_SetDesiredRotationMode(NewRotMode);
    }
}

void AGSHeroCharacter::Server_SetDesiredRotationMode_Implementation(EALSRotationMode NewRotMode)
{
    SetDesiredRotationMode(NewRotMode);
}

void AGSHeroCharacter::SetRotationMode(const EALSRotationMode NewRotationMode)
{
    if (RotationMode != NewRotationMode)
    {
        const EALSRotationMode Prev = RotationMode;
        RotationMode = NewRotationMode;
        OnRotationModeChanged(Prev);

        if (GetLocalRole() == ROLE_AutonomousProxy)
        {
            Server_SetRotationMode(NewRotationMode);
        }
    }
}

void AGSHeroCharacter::Server_SetRotationMode_Implementation(EALSRotationMode NewRotationMode)
{
    SetRotationMode(NewRotationMode);
}

void AGSHeroCharacter::SetViewMode(const EALSViewMode NewViewMode)
{
    if (ViewMode != NewViewMode)
    {
        const EALSViewMode Prev = ViewMode;
        ViewMode = NewViewMode;
        OnViewModeChanged(Prev);

        if (GetLocalRole() == ROLE_AutonomousProxy)
        {
            Server_SetViewMode(NewViewMode);
        }
    }
}

void AGSHeroCharacter::Server_SetViewMode_Implementation(EALSViewMode NewViewMode)
{
    SetViewMode(NewViewMode);
}

void AGSHeroCharacter::SetOverlayState(const EALSOverlayState NewState)
{
    if (OverlayState != NewState)
    {
        const EALSOverlayState Prev = OverlayState;
        OverlayState = NewState;
        OnOverlayStateChanged(Prev);

        if (GetLocalRole() == ROLE_AutonomousProxy)
        {
            Server_SetOverlayState(NewState);
        }
    }
}


void AGSHeroCharacter::Server_SetOverlayState_Implementation(EALSOverlayState NewState)
{
    SetOverlayState(NewState);
}

/** ALS - State Changes */

void AGSHeroCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

    // Use the Character Movement Mode changes to set the Movement States
    // to the right values.
    //
    // This allows you to have a custom set of movement states but
    // still use the functionality of the default character movement component.

    if (GetCharacterMovement()->MovementMode == MOVE_Walking ||
        GetCharacterMovement()->MovementMode == MOVE_NavWalking)
    {
        SetMovementState(EALSMovementState::Grounded);
    }
    else if (GetCharacterMovement()->MovementMode == MOVE_Falling)
    {
        SetMovementState(EALSMovementState::InAir);
    }
}

void AGSHeroCharacter::OnMovementStateChanged(const EALSMovementState PreviousState)
{
    if (MovementState == EALSMovementState::InAir)
    {
        if (MovementAction == EALSMovementAction::None)
        {
            // If the character enters the air,
            // set the In Air Rotation and uncrouch if crouched.
            InAirRotation = GetActorRotation();
            if (Stance == EALSStance::Crouching)
            {
                UnCrouch();
            }
        }
        //else
        //if (MovementAction == EALSMovementAction::Rolling)
        //{
        //    // If the character is currently rolling, enable the ragdoll.
        //    ReplicatedRagdollStart();
        //}
    }

    //if (CameraBehavior)
    //{
    //    CameraBehavior->MovementState = MovementState;
    //}
}

void AGSHeroCharacter::OnMovementActionChanged(const EALSMovementAction PreviousAction)
{
    // Make the character crouch if performing a roll.
    if (MovementAction == EALSMovementAction::Rolling)
    {
        Crouch();
    }

    if (PreviousAction == EALSMovementAction::Rolling)
    {
        if (DesiredStance == EALSStance::Standing)
        {
            UnCrouch();
        }
        else if (DesiredStance == EALSStance::Crouching)
        {
            Crouch();
        }
    }

    //if (CameraBehavior)
    //{
    //    CameraBehavior->MovementAction = MovementAction;
    //}
}

void AGSHeroCharacter::OnStanceChanged(const EALSStance PreviousStance)
{
    MainAnimInstance->Stance = Stance;

    //if (CameraBehavior)
    //{
    //    CameraBehavior->Stance = Stance;
    //}
}

void AGSHeroCharacter::OnRotationModeChanged(EALSRotationMode PreviousRotationMode)
{
    MainAnimInstance->RotationMode = RotationMode;

    if (RotationMode == EALSRotationMode::VelocityDirection && ViewMode == EALSViewMode::FirstPerson)
    {
        // If the new rotation mode is Velocity Direction and the character is in First Person,
        // set the viewmode to Third Person.
        SetViewMode(EALSViewMode::ThirdPerson);
    }

    //if (CameraBehavior)
    //{
    //    CameraBehavior->SetRotationMode(RotationMode);
    //}
}

void AGSHeroCharacter::OnGaitChanged(const EALSGait PreviousGait)
{
    MainAnimInstance->Gait = Gait;

    //if (CameraBehavior)
    //{
    //    CameraBehavior->Gait = Gait;
    //}
}

void AGSHeroCharacter::OnViewModeChanged(const EALSViewMode PreviousViewMode)
{
    MainAnimInstance->GetCharacterInformationMutable().ViewMode = ViewMode;
    if (ViewMode == EALSViewMode::ThirdPerson)
    {
        if (RotationMode == EALSRotationMode::VelocityDirection || RotationMode == EALSRotationMode::LookingDirection)
        {
            // If Third Person, set the rotation mode back to the desired mode.
            SetRotationMode(DesiredRotationMode);
        }
    }
    else if (ViewMode == EALSViewMode::FirstPerson && RotationMode == EALSRotationMode::VelocityDirection)
    {
        // If First Person, set the rotation mode to looking direction
        // if currently in the velocity direction mode.
        SetRotationMode(EALSRotationMode::LookingDirection);
    }

    //if (CameraBehavior)
    //{
    //    CameraBehavior->ViewMode = ViewMode;
    //}
}

void AGSHeroCharacter::OnOverlayStateChanged(const EALSOverlayState PreviousState)
{
    MainAnimInstance->OverlayState = OverlayState;
}

void AGSHeroCharacter::SetEssentialValues(float DeltaTime)
{
    if (GetLocalRole() != ROLE_SimulatedProxy)
    {
        ReplicatedCurrentAcceleration = GetCharacterMovement()->GetCurrentAcceleration();
        ReplicatedControlRotation = GetControlRotation();
        EasedMaxAcceleration = GetCharacterMovement()->GetMaxAcceleration();
    }
    else
    {
        EasedMaxAcceleration = GetCharacterMovement()->GetMaxAcceleration() != 0
                                   ? GetCharacterMovement()->GetMaxAcceleration()
                                   : EasedMaxAcceleration / 2;
    }

    // Interp AimingRotation to current control rotation
    // for smooth character rotation movement.
    //
    // Decrease InterpSpeed for slower but smoother movement.
    AimingRotation = FMath::RInterpTo(AimingRotation, ReplicatedControlRotation, DeltaTime, 30);

    // These values represent how the capsule is moving as well as
    // how it wants to move, and therefore are essential for any
    // data driven animation system.
    //
    // They are also used throughout the system for various functions,
    // so I found it is easiest to manage them all in one place.

    const FVector CurrentVel = GetVelocity();

    // Set the amount of Acceleration.
    SetAcceleration((CurrentVel - PreviousVelocity) / DeltaTime);

    // Determine if the character is moving by getting it's speed.
    // The Speed equals the length of the horizontal (x y) velocity,
    // so it does not take vertical movement into account.
    //
    // If the character is moving, update the last velocity rotation.
    //
    // This value is saved because it might be useful to know
    // the last orientation of movement even after the character has stopped.

    SetSpeed(CurrentVel.Size2D());
    SetIsMoving(Speed > 1.0f);

    if (bIsMoving)
    {
        LastVelocityRotation = CurrentVel.ToOrientationRotator();
    }

    // Determine if the character has movement input
    // by getting its movement input amount.
    //
    // The Movement Input Amount is equal to the current acceleration
    // divided by the max acceleration so that it has a range of 0-1,
    // 1 being the maximum possible amount of input, and 0 being none.
    //
    // If the character has movement input,
    // update the Last Movement Input Rotation.

    SetMovementInputAmount(ReplicatedCurrentAcceleration.Size() / EasedMaxAcceleration);
    SetHasMovementInput(MovementInputAmount > 0.0f);

    if (bHasMovementInput)
    {
        LastMovementInputRotation = ReplicatedCurrentAcceleration.ToOrientationRotator();
    }

    // Set the Aim Yaw rate by comparing the current and previous Aim Yaw value,
    // divided by Delta Seconds.
    //
    // This represents the speed the camera is rotating left to right.
    SetAimYawRate(FMath::Abs((AimingRotation.Yaw - PreviousAimYaw) / DeltaTime));
}

void AGSHeroCharacter::UpdateCharacterMovement()
{
    // Get the allowed Gait (desired gait with checks)
    const EALSGait AllowedGait = GetAllowedGait();

    // Determine the Actual Gait.
    //
    // If it is different from the current Gait, Set the new Gait Event.

    const EALSGait ActualGait = GetActualGait(AllowedGait);

    if (ActualGait != Gait)
    {
        SetGait(ActualGait);
    }

    // Get the Current Movement Settings and pass it
    // through to the movement component.
    GSMovementComponent->SetMovementSettings(GetTargetMovementSettings());

    // Update the movement state based on current gait
    switch (AllowedGait)
    {
        case EALSGait::Walking:
            GSMovementComponent->StartWalking();
            break;

        case EALSGait::Sprinting:
            GSMovementComponent->StartSprinting();
            break;

        default:
            GSMovementComponent->StopWalking();
            GSMovementComponent->StopSprinting();
            break;
    }
}

void AGSHeroCharacter::UpdateGroundedRotation(float DeltaTime)
{
    if (MovementAction == EALSMovementAction::None)
    {
        const bool bCanUpdateMovingRot = ((bIsMoving && bHasMovementInput) || Speed > 150.0f) && !HasAnyRootMotion();

        if (bCanUpdateMovingRot)
        {
            const float GroundedRotationRate = CalculateGroundedRotationRate();

            if (RotationMode == EALSRotationMode::VelocityDirection)
            {
                // Velocity Direction Rotation
                SmoothCharacterRotation(
                    {0.0f, LastVelocityRotation.Yaw, 0.0f},
                    800.0f,
                    GroundedRotationRate,
                    DeltaTime
                    );
            }
            else
            if (RotationMode == EALSRotationMode::LookingDirection)
            {
                // Looking Direction Rotation
                float YawValue;
                if (Gait == EALSGait::Sprinting)
                {
                    YawValue = LastVelocityRotation.Yaw;
                }
                else
                {
                    // Walking or Running..
                    const float YawOffsetCurveVal = MainAnimInstance->GetCurveValue(FName(TEXT("YawOffset")));
                    YawValue = AimingRotation.Yaw + YawOffsetCurveVal;
                }
                SmoothCharacterRotation({0.0f, YawValue, 0.0f}, 500.0f, GroundedRotationRate, DeltaTime);
            }
            else
            if (RotationMode == EALSRotationMode::Aiming)
            {
                const float ControlYaw = AimingRotation.Yaw;
                SmoothCharacterRotation({0.0f, ControlYaw, 0.0f}, 1000.0f, 20.0f, DeltaTime);
            }
        }
        else
        {
            // Not Moving

            if ((ViewMode == EALSViewMode::ThirdPerson && RotationMode == EALSRotationMode::Aiming) || ViewMode == EALSViewMode::FirstPerson)
            {
                LimitRotation(-100.0f, 100.0f, 20.0f, DeltaTime);
            }

            // Apply the RotationAmount curve from Turn In Place Animations.
            // The Rotation Amount curve defines how much rotation should be
            // applied each frame, and is calculated for animations
            // that are animated at 30fps.

            const float RotAmountCurve = MainAnimInstance->GetCurveValue(FName(TEXT("RotationAmount")));

            if (FMath::Abs(RotAmountCurve) > 0.001f)
            {
                if (GetLocalRole() == ROLE_AutonomousProxy)
                {
                    TargetRotation.Yaw = UKismetMathLibrary::NormalizeAxis(
                        TargetRotation.Yaw + (RotAmountCurve * (DeltaTime / (1.0f / 30.0f))));
                    SetActorRotation(TargetRotation);
                }
                else
                {
                    AddActorWorldRotation({0, RotAmountCurve * (DeltaTime / (1.0f / 30.0f)), 0});
                }
                TargetRotation = GetActorRotation();
            }
        }
    }
    else
    if (MovementAction == EALSMovementAction::Rolling)
    {
        // Rolling Rotation (Not allowed on networked games)
        if (!bEnableNetworkOptimizations && bHasMovementInput)
        {
            SmoothCharacterRotation({0.0f, LastMovementInputRotation.Yaw, 0.0f}, 0.0f, 2.0f, DeltaTime);
        }
    }

    // Other actions are ignored...
}

void AGSHeroCharacter::UpdateInAirRotation(float DeltaTime)
{
    if (RotationMode == EALSRotationMode::VelocityDirection || RotationMode == EALSRotationMode::LookingDirection)
    {
        // Velocity / Looking Direction Rotation
        SmoothCharacterRotation({0.0f, InAirRotation.Yaw, 0.0f}, 0.0f, 5.0f, DeltaTime);
    }
    else if (RotationMode == EALSRotationMode::Aiming)
    {
        // Aiming Rotation
        SmoothCharacterRotation({0.0f, AimingRotation.Yaw, 0.0f}, 0.0f, 15.0f, DeltaTime);
        InAirRotation = GetActorRotation();
    }
}

/** ALS - Utils */

void AGSHeroCharacter::SmoothCharacterRotation(FRotator Target, float TargetInterpSpeed, float ActorInterpSpeed, float DeltaTime)
{
    // Interpolate the Target Rotation for extra smooth rotation behavior
    TargetRotation =
        FMath::RInterpConstantTo(
            TargetRotation,
            Target,
            DeltaTime,
            TargetInterpSpeed
            );

    SetActorRotation(
        FMath::RInterpTo(
            GetActorRotation(),
            TargetRotation,
            DeltaTime,
            ActorInterpSpeed
            )
        );
}

float AGSHeroCharacter::CalculateGroundedRotationRate() const
{
    // Calculate the rotation rate by using the current Rotation Rate Curve
    // in the Movement Settings.
    //
    // Using the curve in conjunction with the mapped speed
    // gives you a high level of control over the rotation
    // rates for each speed.
    //
    // Increase the speed if the camera is rotating quickly for more
    // responsive rotation.

    const float MappedSpeedVal = GSMovementComponent->GetMappedSpeed();
    const float CurveVal =
        GSMovementComponent->CurrentMovementSettings.RotationRateCurve->GetFloatValue(MappedSpeedVal);

    const float ClampedAimYawRate = FMath::GetMappedRangeValueClamped(
        {0.0f, 300.0f},
        {1.0f, 3.0f},
        AimYawRate
        );

    return CurveVal * ClampedAimYawRate;
}

void AGSHeroCharacter::LimitRotation(float AimYawMin, float AimYawMax, float InterpSpeed, float DeltaTime)
{
    // Prevent the character from rotating past a certain angle.
    FRotator Delta = AimingRotation - GetActorRotation();
    Delta.Normalize();
    const float RangeVal = Delta.Yaw;

    if (RangeVal < AimYawMin || RangeVal > AimYawMax)
    {
        const float ControlRotYaw = AimingRotation.Yaw;
        const float TargetYaw = ControlRotYaw + (RangeVal > 0.0f ? AimYawMin : AimYawMax);
        SmoothCharacterRotation(
            {0.0f, TargetYaw, 0.0f},
            0.0f,
            InterpSpeed,
            DeltaTime
            );
    }
}

void AGSHeroCharacter::SetMovementModel()
{
    const FString ContextString = GetFullName();
    FALSMovementStateSettings* OutRow =
        MovementModel.DataTable->FindRow<FALSMovementStateSettings>(MovementModel.RowName, ContextString);
    check(OutRow);
    MovementData = *OutRow;
}

/** ALS - Replication */

void AGSHeroCharacter::OnRep_RotationMode(EALSRotationMode PrevRotMode)
{
    OnRotationModeChanged(PrevRotMode);
}

void AGSHeroCharacter::OnRep_ViewMode(EALSViewMode PrevViewMode)
{
    OnViewModeChanged(PrevViewMode);
}

void AGSHeroCharacter::OnRep_OverlayState(EALSOverlayState PrevOverlayState)
{
    OnOverlayStateChanged(PrevOverlayState);
}

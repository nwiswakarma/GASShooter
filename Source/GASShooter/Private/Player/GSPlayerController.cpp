// Copyright 2020 Dan Kestranek.


#include "Player/GSPlayerController.h"
#include "Characters/Abilities/AttributeSets/GSAmmoAttributeSet.h"
#include "Characters/Abilities/AttributeSets/GSAttributeSetBase.h"
#include "Characters/Abilities/GSAbilitySystemComponent.h"
#include "Characters/Heroes/GSHeroCharacter.h"
#include "Player/GSPlayerState.h"
#include "UI/GSHUD.h"
#include "UI/GSHUDWidget.h"
#include "Weapons/GSWeapon.h"
#include "Weapons/GSUTProjectile.h"
#include "GSBlueprintFunctionLibrary.h"

#include "Kismet/KismetSystemLibrary.h"
#include "PaperSprite.h"

#include "Framework/Application/SlateApplication.h"

AGSPlayerController::AGSPlayerController(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PredictionFudgeFactor = 20.f;
	MaxPredictionPing = 0.f; 
	DesiredPredictionPing = 120.f;
	bIsDebuggingProjectiles = false;
}

void AGSPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AGSPlayerController, PredictionFudgeFactor, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AGSPlayerController, MaxPredictionPing, COND_OwnerOnly);
}

void AGSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (GetLocalRole() < ROLE_Authority)
	{
		ServerNegotiatePredictionPing(DesiredPredictionPing);

        //FSlateApplication::Get().OnApplicationActivationStateChanged()
        //    .AddUObject(this, &AGSPlayerController::OnWindowFocusChanged);
	}
}

//void AGSPlayerController::OnWindowFocusChanged(bool bIsFocused)
//{
//    if (bIsFocused)
//    {
//        // Unlimit game FPS
//        //GEngine->SetMaxFPS( 0 );
//    
//        // Unpause the game
//        // MyHUD->SetPause( false );
//    
//        UE_LOG(LogTemp, Warning, TEXT("FOCUS IN"));
//    }
//    else
//    {
//        // Reduce FPS to max 10 while in the background
//        //GEngine->SetMaxFPS( 10.0f );
//    
//        // Pause the game, using your "show pause menu" function
//        // MyHUD->SetPause( true );
//    
//        UE_LOG(LogTemp, Warning, TEXT("FOCUS OUT"));
//    }
//}

void AGSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    //if (IsLocalPlayerController())
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("CHECK %d %s (%s)"),
    //        IsValid(HeroCharacter),
    //        *FString(__FUNCTION__),
    //        *UGSBlueprintFunctionLibrary::GetPlayerEditorWindowRole(GetWorld()));
    //}

    if (IsLocalPlayerController() &&
        IsValid(HeroCharacter) &&
        HeroCharacter->IsUsingAimInput())
    {
        UpdatePawnControlProjection(DeltaTime);
    }
}

void AGSPlayerController::UpdatePawnControlProjection(float DeltaTime)
{
    check(IsValid(HeroCharacter));
    check(IsLocalPlayerController());

    UWorld* World = GetWorld();
    check(World);

    UCameraComponent* Camera = HeroCharacter->GetCamera();

    // No assigned camera, return
    if (! Camera)
    {
        return;
    }

    check(IsValid(Camera));

    // World to screen projection flag
    bool bProjectionSuccess = false;

    const FVector ProjectionOffset = HeroCharacter->GetProjectionAnchorOffset();
    const FVector ProjectionNormal = HeroCharacter->GetActorUpVector();
    const FVector ProjectionAnchor = HeroCharacter->GetActorLocation() + ProjectionOffset*ProjectionNormal;

    const FPlane ProjectionPlane(ProjectionAnchor, ProjectionNormal);

    // Clamp mouse position within viewport

    // Viewport size (no DPI scale) and DPI scale
    FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
    float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);

    // Anchor screen position (no DPI scale)
    FVector2D ViewportAnchor;
    bProjectionSuccess = UGameplayStatics::ProjectWorldToScreen(
        this,
        ProjectionAnchor,
        ViewportAnchor,
        true
        );

    // Projection failed, possibly due to target world location
    // is out of camera (commonly during client initial possession).
    //
    // Reset camera location and abort.
    if (! bProjectionSuccess)
    {
        Camera->SetRelativeLocation(FVector::ZeroVector);
        return;
    }

    // Mouse position (no DPI scale) on viewport
    FVector2D MouseViewportPos(UWidgetLayoutLibrary::GetMousePositionOnViewport(World));

    // Viewport anchor to mouse delta
    FVector2D ViewportAnchorToMouse = (MouseViewportPos-ViewportAnchor);

    // Clamp mouse position only if they are out viewport
    if (bClampMousePosition && (
        MouseViewportPos.X < 0.f ||
        MouseViewportPos.Y < 0.f ||
        MouseViewportPos.X > ViewportSize.X ||
        MouseViewportPos.Y > ViewportSize.Y
        ) )
    {
        float cx = FMath::Clamp(MouseViewportPos.X, 0.f, ViewportSize.X);
        float cy = FMath::Clamp(MouseViewportPos.Y, 0.f, ViewportSize.Y);

        // SetMouseLocation require DPI scaled viewport mouse position
        SetMouseLocation(
            static_cast<int32>(cx*ViewportScale),
            static_cast<int32>(cy*ViewportScale)
            );

        MouseViewportPos.X = cx;
        MouseViewportPos.Y = cy;
    }

    // Calculate aim rotation

    FVector MouseWorldPos;
    FVector MouseWorldDir;

    UGameplayStatics::DeprojectScreenToWorld(
        this,
        MouseViewportPos,
        MouseWorldPos,
        MouseWorldDir
        );

    FVector MouseProjected = FMath::RayPlaneIntersection(
        MouseWorldPos,
        MouseWorldDir,
        ProjectionPlane
        );

    FVector MouseProjectionDelta = MouseProjected-ProjectionAnchor;

    if (MouseProjectionDelta.SizeSquared() > 0.f)
    {
        FVector AimDir(MouseProjectionDelta.GetUnsafeNormal());
        FRotator AimRot(AimDir.ToOrientationRotator());

        AimRot = FRotator(0, AimRot.Yaw, 0);

        // Only update if yaw rotation changed
        if (! FMath::IsNearlyEqual(AimRot.Yaw, HeroCharacter->GetAimingRotation().Yaw))
        {
            HeroCharacter->SetAimingRotation(AimRot);
            SetControlRotation(AimRot);
        }

        // Update projected aim screen location

        FVector WeaponAnchorWorld(HeroCharacter->GetWeaponAttachPointLocation());
        FVector2D WeaponAnchorScreen;

        UGameplayStatics::ProjectWorldToScreen(
            this,
            WeaponAnchorWorld,
            WeaponAnchorScreen,
            true
            );

        // Projection failed, possibly due to target world location
        // is out of camera (commonly during client initial possession).
        //
        // Reset camera location and abort.
        if (! bProjectionSuccess)
        {
            Camera->SetRelativeLocation(FVector::ZeroVector);
            return;
        }

        // Calculate screen location closest to the mouse cursor
        // and weapon aim screen projected line
        const FVector2D WeaponDir = ViewportAnchorToMouse.GetSafeNormal();
        ProjectedAimScreenLocation = WeaponAnchorScreen + (WeaponDir * ((MouseViewportPos-WeaponAnchorScreen) | WeaponDir));
    }

    // Reprojected camera position

    FVector CameraWorldPos = Camera->GetComponentLocation();
    FVector CameraWorldDir = Camera->GetForwardVector();

    FVector CameraProjected = FMath::RayPlaneIntersection(
        CameraWorldPos,
        CameraWorldDir,
        ProjectionPlane
        );

    // Calculate camera view projection

    FVector ViewSrc = CameraProjected;
    FVector ViewDst = ViewSrc;

    {
        // Viewport extents
        FVector2D VSExts = (ViewportSize*.5f);

        // Viewport target position
        //
        // View target position is half the distance between viewport anchor to mouse position.
        // The distance is relative to the size of viewport with span.

        FVector2D ViewportSizeWithSpan = (ViewportSize+ViewportSpanSize);
        FVector2D VSWithSpanRatio = ViewportSize/ViewportSizeWithSpan;

        FVector2D PosRatio = ViewportAnchorToMouse/ViewportSizeWithSpan;
        PosRatio.X = FMath::Clamp(PosRatio.X, -VSWithSpanRatio.X, VSWithSpanRatio.X);
        PosRatio.Y = FMath::Clamp(PosRatio.Y, -VSWithSpanRatio.Y, VSWithSpanRatio.Y);

        FVector2D ViewTargetPos = PosRatio*VSExts;

        // Clamp view target to viewport extents for safety

        if (FMath::Abs(ViewTargetPos.X) > VSExts.X)
        {
            ViewTargetPos.X = ViewTargetPos.X > 0.f
                ? VSExts.X
                : -VSExts.X;
        }

        if (FMath::Abs(ViewTargetPos.Y) > VSExts.Y)
        {
            ViewTargetPos.Y = ViewTargetPos.Y > 0.f
                ? VSExts.Y
                : -VSExts.Y;
        }

        // Deproject view target position

        FVector2D ViewTargetOnAnchorPos = ViewportAnchor+ViewTargetPos;

        FVector ViewTargetWorldPos;
        FVector ViewTargetWorldDir;

        UGameplayStatics::DeprojectScreenToWorld(
            this,
            ViewTargetOnAnchorPos,
            ViewTargetWorldPos,
            ViewTargetWorldDir
            );

        ViewDst = FMath::RayPlaneIntersection(
            ViewTargetWorldPos,
            ViewTargetWorldDir,
            ProjectionPlane
            );
    }

    // Interpolate destination view position
    FVector ViewInterpPos(
        FMath::FInterpTo(ViewSrc.X, ViewDst.X, DeltaTime, CameraInterpSpeed),
        FMath::FInterpTo(ViewSrc.Y, ViewDst.Y, DeltaTime, CameraInterpSpeed),
        FMath::FInterpTo(ViewSrc.Z, ViewDst.Z, DeltaTime, CameraInterpSpeed)
        );

    // Move camera
    Camera->SetWorldLocation(CameraWorldPos+(ViewInterpPos-CameraProjected));
}

void AGSPlayerController::CreateHUD()
{
    // Only create once
    if (UIHUDWidget)
    {
        return;
    }

    if (!UIHUDWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("%s() Missing UIHUDWidgetClass. Please fill in on the Blueprint of the PlayerController."), *FString(__FUNCTION__));
        return;
    }

    // Only create a HUD for local player
    if (!IsLocalPlayerController())
    {
        return;
    }

    UIHUDWidget = CreateWidget<UGSHUDWidget>(this, UIHUDWidgetClass);
    UIHUDWidget->AddToViewport();

    // Update HUD information
    UpdateHUD();
}

void AGSPlayerController::UpdateHUD()
{
    // Only update if HUD widget have already been created
    if (! UIHUDWidget)
    {
        return;
    }

    // Only create a HUD for local player
    if (!IsLocalPlayerController())
    {
        return;
    }

    // Need a valid PlayerState to get attributes from
    AGSPlayerState* PS = GetPlayerState<AGSPlayerState>();
    if (!PS)
    {
        return;
    }

    // Set attributes
    UIHUDWidget->SetCurrentHealth(PS->GetHealth());
    UIHUDWidget->SetMaxHealth(PS->GetMaxHealth());
    UIHUDWidget->SetHealthPercentage(PS->GetHealth() / PS->GetMaxHealth());
    UIHUDWidget->SetCurrentMana(PS->GetMana());
    UIHUDWidget->SetMaxMana(PS->GetMaxMana());
    UIHUDWidget->SetManaPercentage(PS->GetMana() / PS->GetMaxMana());
    UIHUDWidget->SetHealthRegenRate(PS->GetHealthRegenRate());
    UIHUDWidget->SetManaRegenRate(PS->GetManaRegenRate());
    UIHUDWidget->SetCurrentStamina(PS->GetStamina());
    UIHUDWidget->SetMaxStamina(PS->GetMaxStamina());
    UIHUDWidget->SetStaminaPercentage(PS->GetStamina() / PS->GetMaxStamina());
    UIHUDWidget->SetStaminaRegenRate(PS->GetStaminaRegenRate());
    UIHUDWidget->SetCurrentShield(PS->GetShield());
    UIHUDWidget->SetMaxShield(PS->GetMaxShield());
    UIHUDWidget->SetShieldRegenRate(PS->GetShieldRegenRate());
    UIHUDWidget->SetShieldPercentage(PS->GetShield() / PS->GetMaxShield());
    UIHUDWidget->SetExperience(PS->GetXP());
    UIHUDWidget->SetGold(PS->GetGold());
    UIHUDWidget->SetHeroLevel(PS->GetCharacterLevel());

    if (HeroCharacter)
    {
        AGSWeapon* CurrentWeapon = HeroCharacter->GetCurrentWeapon();

        if (CurrentWeapon)
        {
            UIHUDWidget->SetEquippedWeaponSprite(CurrentWeapon->PrimaryIcon);
            UIHUDWidget->SetEquippedWeaponStatusText(CurrentWeapon->GetDefaultStatusText());
            UIHUDWidget->SetPrimaryClipAmmo(HeroCharacter->GetPrimaryClipAmmo());
            UIHUDWidget->SetReticle(CurrentWeapon->GetPrimaryHUDReticleClass());

            // PlayerState's Pawn isn't set up yet so we can't just call PS->GetPrimaryReserveAmmo()
            if (PS->GetAmmoAttributeSet())
            {
                FGameplayAttribute Attribute = PS->GetAmmoAttributeSet()->GetReserveAmmoAttributeFromTag(CurrentWeapon->PrimaryAmmoType);
                if (Attribute.IsValid())
                {
                    UIHUDWidget->SetPrimaryReserveAmmo(PS->GetAbilitySystemComponent()->GetNumericAttribute(Attribute));
                }
            }
        }
    }
}

UGSHUDWidget* AGSPlayerController::GetGSHUD()
{
    return UIHUDWidget;
}

void AGSPlayerController::SetEquippedWeaponPrimaryIconFromSprite(UPaperSprite* InSprite)
{
    if (UIHUDWidget)
    {
        UIHUDWidget->SetEquippedWeaponSprite(InSprite);
    }
}

void AGSPlayerController::SetEquippedWeaponStatusText(const FText& StatusText)
{
    if (UIHUDWidget)
    {
        UIHUDWidget->SetEquippedWeaponStatusText(StatusText);
    }
}

void AGSPlayerController::SetPrimaryClipAmmo(int32 ClipAmmo)
{
    if (UIHUDWidget)
    {
        UIHUDWidget->SetPrimaryClipAmmo(ClipAmmo);
    }
}

void AGSPlayerController::SetPrimaryReserveAmmo(int32 ReserveAmmo)
{
    if (UIHUDWidget)
    {
        UIHUDWidget->SetPrimaryReserveAmmo(ReserveAmmo);
    }
}

void AGSPlayerController::SetSecondaryClipAmmo(int32 SecondaryClipAmmo)
{
    if (UIHUDWidget)
    {
        UIHUDWidget->SetSecondaryClipAmmo(SecondaryClipAmmo);
    }
}

void AGSPlayerController::SetSecondaryReserveAmmo(int32 SecondaryReserveAmmo)
{
    if (UIHUDWidget)
    {
        UIHUDWidget->SetSecondaryReserveAmmo(SecondaryReserveAmmo);
    }
}

void AGSPlayerController::SetHUDReticle(TSubclassOf<UGSHUDReticle> ReticleClass)
{
    // !GetWorld()->bIsTearingDown Stops an error when quitting PIE while targeting when the EndAbility resets the HUD reticle
    if (UIHUDWidget && GetWorld() && !GetWorld()->bIsTearingDown)
    {
        UIHUDWidget->SetReticle(ReticleClass);
    }
}

void AGSPlayerController::ShowDamageNumber_Implementation(float DamageAmount, AGSCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags)
{
    if (IsValid(TargetCharacter))
    {
        TargetCharacter->AddDamageNumber(DamageAmount, DamageNumberTags);
    }
}

bool AGSPlayerController::ShowDamageNumber_Validate(float DamageAmount, AGSCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags)
{
    return true;
}

void AGSPlayerController::SetRespawnCountdown_Implementation(float RespawnTimeRemaining)
{
    if (UIHUDWidget)
    {
        UIHUDWidget->SetRespawnCountdown(RespawnTimeRemaining);
    }
}

bool AGSPlayerController::SetRespawnCountdown_Validate(float RespawnTimeRemaining)
{
    return true;
}

void AGSPlayerController::ClientSetControlRotation_Implementation(FRotator NewRotation)
{
    SetControlRotation(NewRotation);
}

bool AGSPlayerController::ClientSetControlRotation_Validate(FRotator NewRotation)
{
    return true;
}

void AGSPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    AGSPlayerState* PS = GetPlayerState<AGSPlayerState>();
    if (PS)
    {
        // Init ASC with PS (Owner) and our new Pawn (AvatarActor)
        PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, InPawn);
    }

    // Create HUD if on standalone mode
    if (UKismetSystemLibrary::IsStandalone(this))
    {
        CreateHUD();
    }
}

void AGSPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    // For edge cases where the PlayerState is repped before the Hero is possessed.
    CreateHUD();
}

void AGSPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);

    HeroCharacter = Cast<AGSHeroCharacter>(P);

    //UE_LOG(LogTemp, Warning, TEXT("CHECK %d %s (%s)"),
    //    IsValid(HeroCharacter),
    //    *FString(__FUNCTION__),
    //    *UGSBlueprintFunctionLibrary::GetPlayerEditorWindowRole(GetWorld()));

    // Update HUD information if there is valid character
    if (IsValid(HeroCharacter))
    {
        UpdateHUD();
    }
}

void AGSPlayerController::Kill()
{
    ServerKill();
}

void AGSPlayerController::ServerKill_Implementation()
{
    AGSPlayerState* PS = GetPlayerState<AGSPlayerState>();
    if (PS)
    {
        PS->GetAttributeSetBase()->SetHealth(0.0f);
    }
}

bool AGSPlayerController::ServerKill_Validate()
{
    return true;
}

FVector2D AGSPlayerController::GetProjectedAimScreenLocation()
{
    return ProjectedAimScreenLocation;
}

int32 AGSPlayerController::GetPing() const
{
    if (PlayerState && (GetNetMode() != NM_Standalone))
    {
        return PlayerState->GetPing();
    }
    else
    {
        return -1;
    }
}

int32 AGSPlayerController::GetExactPing() const
{
    if (PlayerState && (GetNetMode() != NM_Standalone))
    {
        return PlayerState->ExactPing;
    }
    else
    {
        return -1;
    }
}

float AGSPlayerController::GetPredictionTime()
{
    // exact ping is in msec, divide by 1000 to get time in seconds
    //if (Role == ROLE_Authority) { UE_LOG(UT, Warning, TEXT("Server ExactPing %f"), PlayerState->ExactPing); }
    return (PlayerState && (GetNetMode() != NM_Standalone))
        ? (0.0005f*FMath::Clamp(PlayerState->ExactPing - PredictionFudgeFactor, 0.f, MaxPredictionPing))
        : 0.f;
}

float AGSPlayerController::GetProjectileSleepTime()
{
	return 0.001f * FMath::Max(0.f, PlayerState->ExactPing - PredictionFudgeFactor - MaxPredictionPing);
}

void AGSPlayerController::ServerNegotiatePredictionPing_Implementation(float NewPredictionPing)
{
	//MaxPredictionPing = FMath::Clamp(NewPredictionPing, 0.f, UUTGameEngine::StaticClass()->GetDefaultObject<UUTGameEngine>()->ServerMaxPredictionPing);
	//MaxPredictionPing = FMath::Clamp(NewPredictionPing, 0.f, 120.f);
	MaxPredictionPing = FMath::Clamp(NewPredictionPing, 0.f, 999.f);
}

bool AGSPlayerController::ServerNegotiatePredictionPing_Validate(float NewPredictionPing)
{
	return true;
}

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

#include "PaperSprite.h"

void AGSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (IsValid(HeroCharacter) && IsLocalPlayerController())
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

    UCameraComponent* Camera = HeroCharacter->GetThirdPersonCamera();

    check(IsValid(Camera));

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
    ProjectWorldLocationToScreen(ProjectionAnchor, ViewportAnchor);

    // Mouse position (no DPI scale) on viewport
    FVector2D MouseViewportPos(UWidgetLayoutLibrary::GetMousePositionOnViewport(World));

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
        FRotator AimRot(MouseProjectionDelta.GetUnsafeNormal().ToOrientationRotator());
        AimRot = FRotator(0, AimRot.Yaw, 0);

        HeroCharacter->SetAimRotation(AimRot);
        SetControlRotation(AimRot);
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
        FVector2D Exts = (ViewportSize*.5f)-ViewportSpanSize;
        FVector2D ViewportAnchorToMouse = MouseViewportPos-ViewportAnchor;

        if (FMath::Abs(ViewportAnchorToMouse.X) > Exts.X)
        {
            ViewportAnchorToMouse.X = ViewportAnchorToMouse.X > 0.f
                ? Exts.X
                : -Exts.X;
        }

        if (FMath::Abs(ViewportAnchorToMouse.Y) > Exts.Y)
        {
            ViewportAnchorToMouse.Y = ViewportAnchorToMouse.Y > 0.f
                ? Exts.Y
                : -Exts.Y;
        }

        FVector2D ViewTargetPos = ViewportAnchor+ViewportAnchorToMouse;

        FVector ViewTargetWorldPos;
        FVector ViewTargetWorldDir;

        UGameplayStatics::DeprojectScreenToWorld(
            this,
            ViewTargetPos,
            ViewTargetWorldPos,
            ViewTargetWorldDir
            );

        ViewDst = FMath::RayPlaneIntersection(
            ViewTargetWorldPos,
            ViewTargetWorldDir,
            ProjectionPlane
            );
    }

    FVector ViewInterpPos(
        FMath::FInterpTo(ViewSrc.X, ViewDst.X, DeltaTime, CameraInterpSpeed),
        FMath::FInterpTo(ViewSrc.Y, ViewDst.Y, DeltaTime, CameraInterpSpeed),
        FMath::FInterpTo(ViewSrc.Z, ViewDst.Z, DeltaTime, CameraInterpSpeed)
        );

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

	// Need a valid PlayerState to get attributes from
	AGSPlayerState* PS = GetPlayerState<AGSPlayerState>();
	if (!PS)
	{
		return;
	}

	UIHUDWidget = CreateWidget<UGSHUDWidget>(this, UIHUDWidgetClass);
	UIHUDWidget->AddToViewport();

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

	HeroCharacter = GetPawn<AGSHeroCharacter>();

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
}

void AGSPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// For edge cases where the PlayerState is repped before the Hero is possessed.
	CreateHUD();
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

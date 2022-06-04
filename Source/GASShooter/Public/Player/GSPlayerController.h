// Copyright 2020 Dan Kestranek.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Characters/GSCharacterBase.h"
#include "GSPlayerController.generated.h"

class UPaperSprite;
class AGSHeroCharacter;

/**
 * 
 */
UCLASS()
class GASSHOOTER_API AGSPlayerController : public APlayerController
{
    GENERATED_BODY()
    
public:
	AGSPlayerController(const class FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void CreateHUD();

    class UGSHUDWidget* GetGSHUD();

    /**
    * Weapon HUD info
    */

    UFUNCTION(BlueprintCallable, Category = "GASShooter|UI")
    void SetEquippedWeaponPrimaryIconFromSprite(UPaperSprite* InSprite);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|UI")
    void SetEquippedWeaponStatusText(const FText& StatusText);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|UI")
    void SetPrimaryClipAmmo(int32 ClipAmmo);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|UI")
    void SetPrimaryReserveAmmo(int32 ReserveAmmo);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|UI")
    void SetSecondaryClipAmmo(int32 SecondaryClipAmmo);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|UI")
    void SetSecondaryReserveAmmo(int32 SecondaryReserveAmmo);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|UI")
    void SetHUDReticle(TSubclassOf<class UGSHUDReticle> ReticleClass);

    UFUNCTION(Client, Reliable, WithValidation)
    void ShowDamageNumber(float DamageAmount, AGSCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags);
    void ShowDamageNumber_Implementation(float DamageAmount, AGSCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags);
    bool ShowDamageNumber_Validate(float DamageAmount, AGSCharacterBase* TargetCharacter, FGameplayTagContainer DamageNumberTags);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|UI")
    FVector2D GetProjectedAimScreenLocation();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GASShooter|Network")
	int32 GetPing() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GASShooter|Network")
	int32 GetExactPing() const;

    // Simple way to RPC to the client the countdown until they respawn from the GameMode. Will be latency amount of out sync with the Server.
    UFUNCTION(Client, Reliable, WithValidation)
    void SetRespawnCountdown(float RespawnTimeRemaining);
    void SetRespawnCountdown_Implementation(float RespawnTimeRemaining);
    bool SetRespawnCountdown_Validate(float RespawnTimeRemaining);

    UFUNCTION(Client, Reliable, WithValidation)
    void ClientSetControlRotation(FRotator NewRotation);
    void ClientSetControlRotation_Implementation(FRotator NewRotation);
    bool ClientSetControlRotation_Validate(FRotator NewRotation);

	/** Return amount of time to tick or simulate to make up for network lag */
	virtual float GetPredictionTime();

	/** How long fake projectile should sleep before starting to simulate
     * (because client ping is greater than MaxPredictionPing). */
	virtual float GetProjectileSleepTime();

	FORCEINLINE TArray<class AGSUTProjectile*>& GetFakeProjectiles()
    {
        return FakeProjectiles;
    }

	FORCEINLINE bool IsDebuggingProjectiles() const
    {
        return bIsDebuggingProjectiles;
    }

    float GetMaxPredictionPing() const
    {
        return MaxPredictionPing;
    }

    float GetPredictionFudgeFactor() const
    {
        return PredictionFudgeFactor;
    }

protected:
    
    UPROPERTY()
    AGSHeroCharacter* HeroCharacter = nullptr;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GASShooter|UI")
    TSubclassOf<class UGSHUDWidget> UIHUDWidgetClass;

    UPROPERTY(BlueprintReadWrite, Category = "GASShooter|UI")
    class UGSHUDWidget* UIHUDWidget;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    float CameraInterpSpeed = 1.0f;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    float CameraOutOfBoundsInterpSpeed = 10.0f;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    float MaxCameraDistance = 500.0f;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    float ViewportSpanSize = 50.0f;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    bool bClampMousePosition = false;

    FVector2D ProjectedAimScreenLocation;

    //-----------------------------------------------
    // Perceived latency reduction
    /** Used to correct prediction error. */
    UPROPERTY(EditAnywhere, Replicated, GlobalConfig, Category=Network)
    float PredictionFudgeFactor;

    /** Negotiated max amount of ping to predict ahead for. */
    UPROPERTY(BlueprintReadOnly, Category=Network, Replicated)
    float MaxPredictionPing;

    /** user configurable desired prediction ping
     * (will be negotiated with server. */
    //UPROPERTY(BlueprintReadOnly, GlobalConfig, Category=Network)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, GlobalConfig, Category=Network)
    float DesiredPredictionPing;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = Debug)
    bool bIsDebuggingProjectiles;

	/** List of fake projectiles currently out there for this client */
	UPROPERTY()
	TArray<class AGSUTProjectile*> FakeProjectiles;

    // Server only
    virtual void OnPossess(APawn* InPawn) override;

    virtual void OnRep_PlayerState() override;

    virtual void AcknowledgePossession(class APawn* P) override;

    void UpdateHUD();

    UFUNCTION(Exec)
    void Kill();

    UFUNCTION(Server, Reliable)
    void ServerKill();
    void ServerKill_Implementation();
    bool ServerKill_Validate();

    //UFUNCTION(Client, Reliable)
    //void ClientSyncCharacter(AGSWeapon* InWeapon);

    // Update projected pawn aim control
    virtual void UpdatePawnControlProjection(float DeltaTime);

	/** Propose a desired ping to server */
	UFUNCTION(reliable, server, WithValidation)
	virtual void ServerNegotiatePredictionPing(float NewPredictionPing);

    //void OnWindowFocusChanged(bool bIsFocused);
};

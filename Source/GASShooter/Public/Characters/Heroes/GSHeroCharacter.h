// Copyright 2020 Dan Kestranek.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayEffectTypes.h"
#include "Characters/GSCharacterBase.h"
#include "Characters/Abilities/GSInteractable.h"
#include "Library/ALSCharacterEnumLibrary.h"
#include "Library/ALSCharacterStructLibrary.h"
#include "GSHeroCharacter.generated.h"

class UGameplayEffect;
class UCameraComponent;
class AGSWeapon;
class UGSCharacterMovementComponent;
class UALSCharacterAnimInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponChangedDelegate, AGSWeapon*, LastWeapon, AGSWeapon*, NewWeapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSCharacterWeaponAmmoChangedDelegate, int32, OldValue, int32, NewValue);

UENUM(BlueprintType)
enum class EGSHeroWeaponState : uint8
{
    // 0
    Rifle                   UMETA(DisplayName = "Rifle"),
    // 1
    RifleAiming             UMETA(DisplayName = "Rifle Aiming"),
    // 2
    RocketLauncher          UMETA(DisplayName = "Rocket Launcher"),
    // 3
    RocketLauncherAiming    UMETA(DisplayName = "Rocket Launcher Aiming")
};

USTRUCT()
struct GASSHOOTER_API FGSHeroInventory
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    TArray<AGSWeapon*> Weapons;

    // Consumable items

    // Passive items like armor

    // Door keys

    // Etc
};

/**
 * A player or AI controlled hero character.
 */
UCLASS()
class GASSHOOTER_API AGSHeroCharacter : public AGSCharacterBase, public IGSInteractable
{
    GENERATED_BODY()
    
public:
    AGSHeroCharacter(const class FObjectInitializer& ObjectInitializer);

    FGameplayTag CurrentWeaponTag;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Called to bind functionality to input
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Only called on the Server. Calls before Server's AcknowledgePossession.
    virtual void PossessedBy(AController* NewController) override;

    class UGSFloatingStatusBarWidget* GetFloatingStatusBar();

    // Server handles knockdown - cancel abilities, remove effects, activate knockdown ability
    virtual void KnockDown();

    // Plays knockdown effects for all clients from KnockedDown tag listener on PlayerState
    virtual void PlayKnockDownEffects();

    // Plays revive effects for all clients from KnockedDown tag listener on PlayerState
    virtual void PlayReviveEffects();

    virtual void FinishDying() override;

	// Sprint

	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StartSprinting();

	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StopSprinting();

    /**
    * Getters for character perspective
    **/

    virtual USkeletalMeshComponent* GetMainMesh() const override;

    FVector GetProjectionAnchorOffset() const;

    void SetAimingRotation(FRotator NewAimRotation);

    virtual FRotator GetViewRotation() const override;

    UFUNCTION(BlueprintCallable)
    FRotator GetAimingRotation() const;

    UFUNCTION(BlueprintCallable)
    FVector GetWeaponAttachPointLocation() const;

    UFUNCTION(BlueprintCallable)
    bool IsUsingFixedMovementRotation() const;

    UFUNCTION(BlueprintCallable)
    bool IsUsingAimInput() const;

    /**
    * Weapon functionalities
    **/

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    AGSWeapon* GetCurrentWeapon() const;

    // Adds a new weapon to the inventory.
    // Returns false if the weapon already exists in the inventory, true if it's a new weapon.
    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    bool AddWeaponToInventory(AGSWeapon* NewWeapon, bool bEquipWeapon = false);

    // Removes a weapon from the inventory.
    // Returns true if the weapon exists and was removed. False if the weapon didn't exist in the inventory.
    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    bool RemoveWeaponFromInventory(AGSWeapon* WeaponToRemove);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    void RemoveAllWeaponsFromInventory();

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    void EquipWeapon(AGSWeapon* NewWeapon);

    UFUNCTION(Server, Reliable)
    void ServerEquipWeapon(AGSWeapon* NewWeapon);
    void ServerEquipWeapon_Implementation(AGSWeapon* NewWeapon);
    bool ServerEquipWeapon_Validate(AGSWeapon* NewWeapon);

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    virtual void NextWeapon();

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    virtual void PreviousWeapon();

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    virtual void UnEquipWeapon();

    FName GetWeaponAttachPoint() const;

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    int32 GetPrimaryClipAmmo() const;

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    int32 GetMaxPrimaryClipAmmo() const;

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    int32 GetPrimaryReserveAmmo() const;

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    int32 GetSecondaryClipAmmo() const;

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    int32 GetMaxSecondaryClipAmmo() const;

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    int32 GetSecondaryReserveAmmo() const;

    UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
    int32 GetNumWeapons() const;

    /**
    * Interactable interface
    */

    /**
    * We can Interact with other heroes when:
    * Knocked Down - to revive them
    */
    virtual bool IsAvailableForInteraction_Implementation(UPrimitiveComponent* InteractionComponent) const override;

    /**
    * How long to interact with this player:
    * Knocked Down - to revive them
    */
    virtual float GetInteractionDuration_Implementation(UPrimitiveComponent* InteractionComponent) const override;

    /**
    * Interaction:
    * Knocked Down - activate revive GA (plays animation)
    */
    virtual void PreInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* InteractionComponent) override;

    /**
    * Interaction:
    * Knocked Down - apply revive GE
    */
    virtual void PostInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* InteractionComponent) override;

    /**
    * Should we wait and who should wait to sync before calling PreInteract():
    * Knocked Down - Yes, client. This will sync the local player's Interact Duration Timer with the knocked down player's
    * revive animation. If we had a picking a player up animation, we could play it on the local player in PreInteract().
    */
    void GetPreInteractSyncType_Implementation(bool& bShouldSync, EAbilityTaskNetSyncType& Type, UPrimitiveComponent* InteractionComponent) const;

    /**
    * Cancel interaction:
    * Knocked Down - cancel revive ability
    */
    virtual void CancelInteraction_Implementation(UPrimitiveComponent* InteractionComponent) override;

    /**
    * Get the delegate for this Actor canceling interaction:
    * Knocked Down - cancel being revived if killed
    */
    FSimpleMulticastDelegate* GetTargetCancelInteractionDelegate(UPrimitiveComponent* InteractionComponent) override;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "GASShooter|GSHeroCharacter")
    FVector StartingThirdPersonMeshLocation;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GASShooter|Abilities")
    float ReviveDuration;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GASShooter|Camera")
    float BaseTurnRate;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GASShooter|Camera")
    float BaseLookUpRate;

    bool bASCInputBound;

    // Set to true when we change the weapon predictively and flip it to false when the Server replicates to confirm.
    // We use this if the Server refused a weapon change ability's activation to ask the Server to sync the client back up
    // with the correct CurrentWeapon.
    bool bChangedWeaponLocally;

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GASShooter|GSHeroCharacter")
    FName WeaponAttachPoint;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GASShooter|UI")
    TSubclassOf<class UGSFloatingStatusBarWidget> UIFloatingStatusBarClass;

    UPROPERTY()
    class UGSFloatingStatusBarWidget* UIFloatingStatusBar;

    UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "GASShooter|UI")
    class UWidgetComponent* UIFloatingStatusBarComponent;

    UPROPERTY(ReplicatedUsing = OnRep_Inventory)
    FGSHeroInventory Inventory;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GASShooter|Inventory")
    TArray<TSubclassOf<AGSWeapon>> DefaultInventoryWeaponClasses;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
    AGSWeapon* CurrentWeapon;

    UPROPERTY()
    class UGSAmmoAttributeSet* AmmoAttributeSet;

    UPROPERTY(BlueprintAssignable, Category = "GASShooter|GSHeroCharacter")
    FWeaponChangedDelegate OnCurrentWeaponChanged;

    UPROPERTY(BlueprintAssignable, Category = "GASShooter|GSHeroCharacter")
    FGSCharacterWeaponAmmoChangedDelegate OnCurrentWeaponPrimaryClipAmmoChanged;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GASShooter|GSHeroCharacter")
    TSubclassOf<UGameplayEffect> KnockDownEffect;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GASShooter|GSHeroCharacter")
    TSubclassOf<UGameplayEffect> ReviveEffect;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "GASShooter|GSHeroCharacter")
    TSubclassOf<UGameplayEffect> DeathEffect;

    FSimpleMulticastDelegate InteractionCanceledDelegate;

    // Cache tags
    FGameplayTag NoWeaponTag;
    FGameplayTag WeaponChangingDelayReplicationTag;
    FGameplayTag WeaponAmmoTypeNoneTag;
    FGameplayTag WeaponAbilityTag;
    FGameplayTag KnockedDownTag;
    FGameplayTag InteractingTag;

    // Attribute changed delegate handles
    FDelegateHandle PrimaryReserveAmmoChangedDelegateHandle;
    FDelegateHandle SecondaryReserveAmmoChangedDelegateHandle;

    // Tag changed delegate handles
    FDelegateHandle WeaponChangingDelayReplicationTagChangedDelegateHandle;

    // Top-down control

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    bool bUseFixedMovementRotation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    FRotator MovementFixedRotation;

    //UPROPERTY(Replicated)
    FRotator AimingRotation;

    UPROPERTY(BlueprintReadOnly)
    FVector ProjectedCameraViewLocation;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    FVector ProjectionAnchorOffset;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    FVector2D ModelFixedBounds2D;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    bool bUseAimInput;

protected:

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void PreInitializeComponents() override;

    virtual void PostInitializeComponents() override;

    virtual void Tick(float DeltaTime) override;

    // Mouse
    void LookUp(float Value);

    // Gamepad
    void LookUpRate(float Value);

    // Mouse
    void Turn(float Value);

    // Gamepad
    void TurnRate(float Value);

    // Mouse + Gamepad
    void MoveForward(float Value);

    // Mouse + Gamepad
    void MoveRight(float Value);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_SetAimingRotation(FRotator NewAimRotation);

    // Creates and initializes the floating status bar for heroes.
    // Safe to call many times because it checks to make sure it only executes once.
    UFUNCTION()
    void InitializeFloatingStatusBar();

    // Client only
    virtual void OnRep_PlayerState() override;
    virtual void OnRep_Controller() override;

    // Called from both SetupPlayerInputComponent and OnRep_PlayerState because of a potential race condition where the PlayerController might
    // call ClientRestart which calls SetupPlayerInputComponent before the PlayerState is repped to the client so the PlayerState would be null in SetupPlayerInputComponent.
    // Conversely, the PlayerState might be repped before the PlayerController calls ClientRestart so the Actor's InputComponent would be null in OnRep_PlayerState.
    void BindASCInput();

    // Server spawns default inventory
    void SpawnDefaultInventory();

    void SetupStartupPerspective();

    bool DoesWeaponExistInInventory(AGSWeapon* InWeapon);

    void SetCurrentWeapon(AGSWeapon* NewWeapon, AGSWeapon* LastWeapon);

    // Unequips the specified weapon. Used when OnRep_CurrentWeapon fires.
    void UnEquipWeapon(AGSWeapon* WeaponToUnEquip);

    // Unequips the current weapon. Used if for example we drop the current weapon.
    void UnEquipCurrentWeapon();

    UFUNCTION()
    virtual void CurrentWeaponPrimaryClipAmmoChanged(int32 OldPrimaryClipAmmo, int32 NewPrimaryClipAmmo);

    UFUNCTION()
    virtual void CurrentWeaponSecondaryClipAmmoChanged(int32 OldSecondaryClipAmmo, int32 NewSecondaryClipAmmo);

    // Attribute changed callbacks
    virtual void CurrentWeaponPrimaryReserveAmmoChanged(const FOnAttributeChangeData& Data);
    virtual void CurrentWeaponSecondaryReserveAmmoChanged(const FOnAttributeChangeData& Data);

    // Tag changed callbacks
    virtual void WeaponChangingDelayReplicationTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    UFUNCTION()
    void OnRep_CurrentWeapon(AGSWeapon* LastWeapon);

    UFUNCTION()
    void OnRep_Inventory();

    void OnAbilityActivationFailed(const UGameplayAbility* FailedAbility, const FGameplayTagContainer& FailTags);
    
    // The CurrentWeapon is only automatically replicated to simulated clients.
    // The autonomous client can use this to request the proper CurrentWeapon from the server when it knows it may be
    // out of sync with it from predictive client-side changes.
    UFUNCTION(Server, Reliable)
    void ServerSyncCurrentWeapon();
    void ServerSyncCurrentWeapon_Implementation();
    bool ServerSyncCurrentWeapon_Validate();
    
    // The CurrentWeapon is only automatically replicated to simulated clients.
    // Use this function to manually sync the autonomous client's CurrentWeapon when we're ready to.
    // This allows us to predict weapon changes (changing weapons fast multiple times in a row so that the server doesn't
    // replicate and clobber our CurrentWeapon).
    UFUNCTION(Client, Reliable)
    void ClientSyncCurrentWeapon(AGSWeapon* InWeapon);
    void ClientSyncCurrentWeapon_Implementation(AGSWeapon* InWeapon);
    bool ClientSyncCurrentWeapon_Validate(AGSWeapon* InWeapon);

protected:

    /** ALS Settings */

	/* Custom movement component*/
	UPROPERTY()
	UGSCharacterMovementComponent* GSMovementComponent;

    /** Input */

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "ALS|Input")
    EALSRotationMode DesiredRotationMode = EALSRotationMode::LookingDirection;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "ALS|Input")
    EALSGait DesiredGait = EALSGait::Running;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "ALS|Input")
    EALSStance DesiredStance = EALSStance::Standing;

    UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
    float LookUpDownRate = 1.25f;

    UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
    float LookLeftRightRate = 1.25f;

    UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
    float RollDoubleTapTimeout = 0.3f;

    UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
    float ViewModeSwitchHoldTime = 0.2f;

    UPROPERTY(Category = "ALS|Input", BlueprintReadOnly)
    int32 TimesPressedStance = 0;

    UPROPERTY(Category = "ALS|Input", BlueprintReadOnly)
    bool bBreakFall = false;

    UPROPERTY(Category = "ALS|Input", BlueprintReadOnly)
    bool bSprintHeld = false;

    /** Camera System */

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
    float ThirdPersonFOV = 90.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
    float FirstPersonFOV = 90.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
    bool bRightShoulder = false;

    /** State Values */

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_OverlayState)
    EALSOverlayState OverlayState = EALSOverlayState::Default;

    /** Movement System */

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|Movement System")
    FDataTableRowHandle MovementModel;

    /** Essential Information */

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    FVector Acceleration = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    bool bIsMoving = false;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    bool bHasMovementInput = false;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    FRotator LastVelocityRotation;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    FRotator LastMovementInputRotation;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    float Speed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    float MovementInputAmount = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    float AimYawRate = 0.0f;

    /** Replicated Essential Information*/

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
    float EasedMaxAcceleration = 0.0f;

    UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Essential Information")
    FVector ReplicatedCurrentAcceleration = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Essential Information")
    FRotator ReplicatedControlRotation = FRotator::ZeroRotator;

    /** State Values */

    UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
    EALSMovementState MovementState = EALSMovementState::None;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
    EALSMovementState PrevMovementState = EALSMovementState::None;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
    EALSMovementAction MovementAction = EALSMovementAction::None;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_RotationMode)
    EALSRotationMode RotationMode = EALSRotationMode::LookingDirection;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
    EALSGait Gait = EALSGait::Walking;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|State Values")
    EALSStance Stance = EALSStance::Standing;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_ViewMode)
    EALSViewMode ViewMode = EALSViewMode::ThirdPerson;

    /** Movement System */

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Movement System")
    FALSMovementStateSettings MovementData;

    /** Rotation System */

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
    FRotator TargetRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
    FRotator InAirRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
    float YawOffset = 0.0f;

    /** Breakfall System */

    // If player hits to the ground with a specified amount of velocity,
    // switch to breakfall state
    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Breakfall System")
    bool bBreakfallOnLand = true;

    // If player hits to the ground with an amount of velocity
    // greater than specified value, switch to breakfall state
    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Breakfall System", meta = (EditCondition = "bBreakfallOnLand"))
    float BreakfallOnLandVelocity = 600.0f;

    /** Ragdoll System */

    // If player hits to the ground with a specified amount of velocity,
    // switch to ragdoll state
    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Ragdoll System")
    bool bRagdollOnLand = false;

    // If player hits to the ground with an amount of velocity
    // greater than specified value, switch to ragdoll state
    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Ragdoll System", meta = (EditCondition = "bRagdollOnLand"))
    float RagdollOnLandVelocity = 1000.0f;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
    bool bRagdollOnGround = false;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
    bool bRagdollFaceUp = false;

    UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
    FVector LastRagdollVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Ragdoll System")
    FVector TargetRagdollLocation = FVector::ZeroVector;

    // Server ragdoll pull force storage
    float ServerRagdollPull = 0.0f;

    // Dedicated server mesh default visibility based anim tick option
    EVisibilityBasedAnimTickOption DefVisBasedTickOp;

    /** Cached Variables */

    FVector PreviousVelocity = FVector::ZeroVector;

    float PreviousAimYaw = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    UALSCharacterAnimInstance* MainAnimInstance = nullptr;

    //UPROPERTY(BlueprintReadOnly)
    //UALSPlayerCameraBehavior* CameraBehavior;

    // Last time the 'first' crouch/roll button is pressed
    float LastStanceInputTime = 0.0f;

    // Last time the camera action button is pressed
    float CameraActionPressedTime = 0.0f;

    // Timer to manage camera mode swap action
    FTimerHandle OnCameraModeSwapTimer;

    // Timer to manage reset of braking friction factor after on landed event
    FTimerHandle OnLandedFrictionResetTimer;

    // Smooth out aiming by interping control rotation
    //FRotator AimingRotation = FRotator::ZeroRotator;

    // We won't use curve based movement and a few other features
    // on networked games
    bool bEnableNetworkOptimizations = false;

public:

    /** Essential Information Getters/Setters */

    UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
    FVector GetAcceleration() const { return Acceleration; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
    void SetAcceleration(const FVector& NewAcceleration);

    UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
    bool IsMoving() const { return bIsMoving; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
    void SetIsMoving(bool bNewIsMoving);

    UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
    FVector GetMovementInput() const { return ReplicatedCurrentAcceleration; }

    UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
    float GetMovementInputAmount() const { return MovementInputAmount; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
    void SetMovementInputAmount(float NewMovementInputAmount);

    UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
    float GetSpeed() const { return Speed; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
    void SetSpeed(float NewSpeed);

    UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
    float GetAimYawRate() const { return AimYawRate; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
    void SetAimYawRate(float NewAimYawRate);

    UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
    void GetControlForwardRightVector(FVector& Forward, FVector& Right) const;

    /** Movement System */

    UFUNCTION(BlueprintGetter, Category = "ALS|Movement System")
    bool HasMovementInput() const { return bHasMovementInput; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
    void SetHasMovementInput(bool bNewHasMovementInput);

    UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
    FALSMovementSettings GetTargetMovementSettings() const;

    UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
    EALSGait GetAllowedGait() const;

    UFUNCTION(BlueprintCallable, Category = "ALS|Movement States")
    EALSGait GetActualGait(EALSGait AllowedGait) const;

    UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
    bool CanSprint() const;

protected:

    /** Character States */

    UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetMovementState(EALSMovementState NewState);

    UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSMovementState GetMovementState() const { return MovementState; }

    UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSMovementState GetPrevMovementState() const { return PrevMovementState; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetMovementAction(EALSMovementAction NewAction);

    UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSMovementAction GetMovementAction() const { return MovementAction; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetStance(EALSStance NewStance);

    UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSStance GetStance() const { return Stance; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetGait(EALSGait NewGait);

    UFUNCTION(BlueprintSetter, Category = "ALS|Input")
    void SetDesiredStance(EALSStance NewStance);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Input")
    void Server_SetDesiredStance(EALSStance NewStance);

    UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetDesiredGait(EALSGait NewGait);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
    void Server_SetDesiredGait(EALSGait NewGait);

    UFUNCTION(BlueprintGetter, Category = "ALS|Input")
    EALSRotationMode GetDesiredRotationMode() const { return DesiredRotationMode; }

    UFUNCTION(BlueprintSetter, Category = "ALS|Input")
    void SetDesiredRotationMode(EALSRotationMode NewRotMode);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
    void Server_SetDesiredRotationMode(EALSRotationMode NewRotMode);

    UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSGait GetGait() const { return Gait; }

    UFUNCTION(BlueprintGetter, Category = "ALS|CharacterStates")
    EALSGait GetDesiredGait() const { return DesiredGait; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetRotationMode(EALSRotationMode NewRotationMode);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
    void Server_SetRotationMode(EALSRotationMode NewRotationMode);

    UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSRotationMode GetRotationMode() const { return RotationMode; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetViewMode(EALSViewMode NewViewMode);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
    void Server_SetViewMode(EALSViewMode NewViewMode);

    UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSViewMode GetViewMode() const { return ViewMode; }

    UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetOverlayState(EALSOverlayState NewState);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
    void Server_SetOverlayState(EALSOverlayState NewState);

    UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSOverlayState GetOverlayState() const { return OverlayState; }

    /** State Changes */

    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

    virtual void OnMovementStateChanged(EALSMovementState PreviousState);

    virtual void OnMovementActionChanged(EALSMovementAction PreviousAction);

    virtual void OnStanceChanged(EALSStance PreviousStance);

    virtual void OnRotationModeChanged(EALSRotationMode PreviousRotationMode);

    virtual void OnGaitChanged(EALSGait PreviousGait);

    virtual void OnViewModeChanged(EALSViewMode PreviousViewMode);

    virtual void OnOverlayStateChanged(EALSOverlayState PreviousState);

    //virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

    //virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

    //virtual void OnJumped_Implementation() override;

    //virtual void Landed(const FHitResult& Hit) override;

    //void OnLandFrictionReset();

    virtual void SetEssentialValues(float DeltaTime);

    void UpdateCharacterMovement();

    void UpdateGroundedRotation(float DeltaTime);

    void UpdateInAirRotation(float DeltaTime);

    /** Utils */

	void SmoothCharacterRotation(
        FRotator Target,
        float TargetInterpSpeed,
        float ActorInterpSpeed,
        float DeltaTime
        );

	float CalculateGroundedRotationRate() const;

	void LimitRotation(
        float AimYawMin,
        float AimYawMax,
        float InterpSpeed,
        float DeltaTime
        );

    void SetMovementModel();

    /** Replication */

    UFUNCTION(Category = "ALS|Replication")
    void OnRep_RotationMode(EALSRotationMode PrevRotMode);

    UFUNCTION(Category = "ALS|Replication")
    void OnRep_ViewMode(EALSViewMode PrevViewMode);

    UFUNCTION(Category = "ALS|Replication")
    void OnRep_OverlayState(EALSOverlayState PrevOverlayState);
};

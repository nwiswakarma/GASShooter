// 

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Characters/Abilities/GSAbilityTypes.h"
#include "GSUTProjectile.generated.h"

/** Replicated movement data of our RootComponent.
* More efficient than engine's FRepMovement
*/
USTRUCT()
struct FRepGSUTProjMovement
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    FVector_NetQuantize LinearVelocity;

    UPROPERTY()
    FVector_NetQuantize Location;

    UPROPERTY()
    FRotator Rotation;

    FRepGSUTProjMovement()
        : LinearVelocity(ForceInit)
        , Location(ForceInit)
        , Rotation(ForceInit)
    {}

    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
    {
        bOutSuccess = true;

        bool bOutSuccessLocal = true;

        // update location, linear velocity
        Location.NetSerialize(Ar, Map, bOutSuccessLocal);
        bOutSuccess &= bOutSuccessLocal;
        Rotation.SerializeCompressed(Ar);
        LinearVelocity.NetSerialize(Ar, Map, bOutSuccessLocal);
        bOutSuccess &= bOutSuccessLocal;

        return true;
    }

    bool operator==(const FRepGSUTProjMovement& Other) const
    {
        if (LinearVelocity != Other.LinearVelocity)
        {
            return false;
        }

        if (Location != Other.Location)
        {
            return false;
        }

        if (Rotation != Other.Rotation)
        {
            return false;
        }
        return true;
    }

    bool operator!=(const FRepGSUTProjMovement& Other) const
    {
        return !(*this == Other);
    }
};

UCLASS(meta = (ChildCanTick))
class GASSHOOTER_API AGSUTProjectile : public AActor//, public IUTResetInterface, public IUTTeamInterface
{
    GENERATED_UCLASS_BODY()

    /** Used for replication of our RootComponent's position and velocity */
    UPROPERTY(ReplicatedUsing = OnRep_GSUTProjReplicatedMovement)
    struct FRepGSUTProjMovement GSUTProjReplicatedMovement;

    /** Used for replication of temporary CustomTimeDilation changes */
    UPROPERTY(ReplicatedUsing = OnRep_Slomo)
    float Slomo;

    /** enables UT optimized movement replication; USE THIS INSTEAD OF bReplicateMovement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Replication)
    bool bReplicateUTMovement;

    /** How long projectile stays slowed down. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    float SlomoTime;

    /** Gameplay effects. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    FGSGameplayEffectContainerSpec GameplayEffectSpecs;

    FTimerHandle SlomoTimerHandle;

    UFUNCTION()
    virtual void EndSlomo();

    UFUNCTION()
    virtual void OnRep_Slomo();

    /** GSUTProjReplicatedMovement struct replication event */
    UFUNCTION()
    virtual void OnRep_GSUTProjReplicatedMovement();

    virtual void GatherCurrentMovement() override;

    virtual bool DisableEmitterLights() const;

    virtual void OnRep_Instigator() override;

    /** Sphere collision component */
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Projectile)
    USphereComponent* CollisionComp;

    /** Projectile movement component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile)
    UProjectileMovementComponent* ProjectileMovement;

    /** additional Z axis speed added to projectile on spawn - NOTE: blueprint changes only work in defaults or construction script as value is applied to velocity on spawn */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Projectile)
    float TossZ;

    /** Percentage of instigator's velocity imparted to this projectile */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Projectile)
    float InstigatorVelocityPct;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
    FRadialDamageParams DamageParams;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
    TSubclassOf<UDamageType> MyDamageType;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
    float Momentum;

    /** How much stats hit credit to give when this projectile hits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
        float StatsHitCredit;

    /** Set by the firing weapon. */
    UPROPERTY()
        FName HitsStatsName;

    /** whether the projectile can impact its Instigator (player who fired it) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
    bool bCanHitInstigator;

    /** whether the projectile can impact teammates of the Instigator (player who fired it) */
    UPROPERTY(EditDefaultsOnly, Category = Damage)
        bool bCanHitTeammates;

    /** whether this projectile should notify our best guess of its target that it is coming (for bots' evasion reaction check, etc)
     * generally should be true unless projectile has complex logic that prevents its target from being known initially (e.g. planted mine)
     */
    UPROPERTY(EditDefaultsOnly, Category = AI)
    bool bInitiallyWarnTarget;

    UPROPERTY(BlueprintReadWrite, Category = Projectile)
    AController* InstigatorController;
    /** team ID used when Instigator is dead */
    UPROPERTY(BlueprintReadWrite, Category = Projectile)
    uint8 InstigatorTeamNum;
    /** if not NULL, Controller that gets credit for damage to enemies on the same team as InstigatorController (including damaging itself)
     * this is used to grant two way kill credit for mechanics where the opposition is partially responsible for the damage(e.g.blowing up an enemy's projectile in flight)
     */
    UPROPERTY(BlueprintReadWrite, Category = Projectile)
    AController* FFInstigatorController;
    /** when FFInstigatorController is assigned damage credit, optionally also change the damage type to this (primarily for death message clarity) */
    UPROPERTY(BlueprintReadWrite, Category = Damage)
    TSubclassOf<UDamageType> FFDamageType;

    /** actor we hit directly and already applied damage to */
    UPROPERTY()
    AActor* ImpactedActor;

    /** If true, projectile will trigger special reward announcement if hit player dies. */
    UPROPERTY()
    bool bPendingSpecialReward;

    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
    //TSubclassOf<class AUTImpactEffect> ExplosionEffects;

    /** If true, kill light on other player's projectiles even at higher settings. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
    bool bLowPriorityLight;

    /** Bounce effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    UParticleSystem* BounceEffect;
    /** Sound played when projectile bounces off wall */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    USoundBase* BounceSound;

    /** return base damage and momentum; OtherActor is set for direct hits and NULL when invoking radial damage (explosion) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Projectile)
    FRadialDamageParams GetDamageParams(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const;

    UPROPERTY(BlueprintReadOnly, Category = Projectile)
    USceneComponent* OffsetVisualComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    bool bDoVisualOffset;

    UPROPERTY()
    FVector FinalVisualOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    FVector InitialVisualOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    float OffsetTime;

    virtual void PrepareForIntermission();

    /** this is used with SendInitialReplication() to force projectiles to be initially replicated prior to initial physics,
     * which fixes most cases of projectiles never appearing on clients (because they got blown up before ever being sent)
     * we use a special tick function instead of BeginPlay() so that COND_Initial replication conditions continue to work as expected
     */
    FActorTickFunction InitialReplicationTick;

    /** send initial replication to all clients for which the projectile is relevant, called via InitialReplicationTick */
    void SendInitialReplication();

    /** InstigatedBy received a notification of a client-side hit of this projectile */
    virtual void NotifyClientSideHit(class APlayerController* InstigatedBy, FVector HitLocation, AActor* DamageCauser, int32 Damage);

    /** set to force bReplicatedMovement for the next replication pass; this is used with SendInitialReplication() to make sure the client receives the post-physics location in addition
     * to spawning with the pre-physics location
     */
    bool bForceNextRepMovement;

    /** re-entrancy guard */
    bool bInOverlap;

    /** Fake client side projectile (spawned on owning client). */
    UPROPERTY()
    bool bFakeClientProjectile;

    /** If true (usually), move fake to server replicated position. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Projectile)
    bool bMoveFakeToReplicatedPos;

    /** If true, explode instead of bouncing off damageable geometry. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Projectile)
    bool bDamageOnBounce;

    /** Fake projectile on this client providing visuals for me */
    UPROPERTY()
    AGSUTProjectile* MyFakeProjectile;

    /** Real projectile for which this projectile is providing visuals */
    UPROPERTY()
    AGSUTProjectile* MasterProjectile;

    /** True once fully spawned, to avoid destroying replicated projectiles during spawn on client */
    UPROPERTY()
    bool bHasSpawnedFully;

    /** Return true if InFakeProjectile is a possible match for this projectile. */
    virtual bool CanMatchFake(AGSUTProjectile* InFakeProjectile, const FVector& VelDir) const;

    /** Perform any custom initialization for this projectile as fake client side projectile */
    virtual void InitFakeProjectile(class AGSPlayerController* OwningPlayer);

    /** Synchronize replicated projectile with the associated client-side fake projectile */
    virtual void BeginFakeProjectileSynch(AGSUTProjectile* InFakeProjectile);

    /** Server catchup ticking for client's projectile */
    virtual void CatchupTick(float CatchupTickDelta);

    /** Called if server isn't ticking this projectile to make up for network latency. */
    virtual void SetForwardTicked(bool bWasForwardTicked) {};

    /** true if already exploded (to avoid recursion, etc) */
    bool bExploded;

    virtual void PreInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void TornOff() override;
    virtual void Destroyed() override;

    /** simulate LifeSpan on the client side
     * this is harmless because the Destroy() call won't work if the projectile isn't torn off
     * and makes simulating lifetime related projectile effects easier on clients
     */
    virtual void SetLifeSpan(float InLifespan) override
    {
        // Store the new value
        InitialLifeSpan = InLifespan;
        if (InLifespan > 0.0f)
        {
            GetWorldTimerManager().SetTimer(TimerHandle_LifeSpanExpired, this, &AActor::LifeSpanExpired, InLifespan);
        }
        else
        {
            GetWorldTimerManager().ClearTimer(TimerHandle_LifeSpanExpired);
        }
    }

    virtual void PostNetReceiveLocationAndRotation() override;
    virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
    virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker) override;

    virtual FVector GetVelocity() const
    {
        if (RootComponent != NULL && (RootComponent->IsSimulatingPhysics() || ProjectileMovement == NULL))
        {
            return GetRootComponent()->GetComponentVelocity();
        }
        else
        {
            return ProjectileMovement->Velocity;
        }
    }

    virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

    /** turns off projectile ambient effects, collision, physics, etc
     * needed because we need a delay between explosion and actor destruction for replication purposes
     */
    UFUNCTION(BlueprintCallable, Category = Projectile)
    virtual void ShutDown();
    /** blueprint hook for shutdown in case any blueprint-created effects need to be turned off */
    UFUNCTION(BlueprintImplementableEvent)
    void OnShutdown();

    //virtual void Reset_Implementation() override
    //{
    //    Destroy();
    //}

    /** returns whether the projectile should ignore a potential overlap hit and keep going
     * split from ProcessHit() as some projectiles want custom hit behavior but the same exclusions
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Projectile)
    bool ShouldIgnoreHit(AActor* OtherActor, UPrimitiveComponent* OtherComp);

    /** called when projectile hits something */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Projectile)
    void ProcessHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);
    /** deal damage to Actor directly hit (note that this Actor will then be ignored for any radial damage) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Projectile)
    void DamageImpactedActor(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);
    UFUNCTION()
    virtual void OnStop(const FHitResult& Hit);
    //UFUNCTION()
    //virtual void OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity);
    UFUNCTION()
    virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    UFUNCTION()
    virtual void OnPawnSphereOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Projectile)
    void Explode(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp = NULL);

    /** Whether this projectile always interacts with other projectiles */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    bool bAlwaysShootable;

    /** If true, this projectile always interacts with other energy projectiles */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
    bool bIsEnergyProjectile;

    UFUNCTION(BlueprintCallable, Category = Projectile)
    virtual bool InteractsWithProj(AGSUTProjectile* OtherProj);

    /* Position of shooter when this projectile was shot. */
    UPROPERTY()
    FVector ShooterLocation;

    UPROPERTY()
    FRotator ShooterRotation;

    /** Projectile size for hitting pawns
     * if set to zero, the extra component used for this feature will not be attached (perf improvement) but means you can't go from 0 at spawn -> 1+ after spawn
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
    float OverlapRadius;

    /** Overlap sphere for hitting pawns
     * NOTE: intentionally hidden from defaults editor so users don't mistakenly modify this when they meant to touch OverlapRadius
     */
    UPROPERTY(BlueprintReadOnly, Category = Projectile)
    USphereComponent* PawnOverlapSphere;

    /** get time to target from current location */
    virtual float GetTimeToLocation(const FVector& TargetLoc) const;
    /** version of GetTimeToLocation() safe to call on default objects */
    virtual float StaticGetTimeToLocation(const FVector& TargetLoc, const FVector& StartLoc) const;

    /** get maximum damage radius this projectile can cause with any explosion type
     * i.e. if the projectile can explode in multiple ways (normal + combo, for instance), return the greater value
     * this is used by AI as part of determining projectile threat and dodge distance
     */
    UFUNCTION(BlueprintNativeEvent)
    float GetMaxDamageRadius() const;

    //virtual uint8 GetTeamNum() const override
    //{
    //    IUTTeamInterface* InstigatorTeamInt = Cast<IUTTeamInterface>(Instigator);
    //    return (InstigatorTeamInt != nullptr) ? InstigatorTeamInt->GetTeamNum() : InstigatorTeamNum;
    //}

    //virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
    //{}
};

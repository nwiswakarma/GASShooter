// Copyright 2020 Dan Kestranek.

#include "Characters/GSCharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Characters/Abilities/GSAbilitySystemGlobals.h"
#include "Characters/GSCharacterBase.h"
#include "GameplayTagContainer.h"

/** === Network Prediction Data === */

void UGSCharacterMovementComponent::FGSSavedMove::Clear()
{
    Super::Clear();

    SavedRequestToStartWalking = false;
    SavedRequestToStartSprinting = false;
    SavedRequestToStartADS = false;
}

uint8 UGSCharacterMovementComponent::FGSSavedMove::GetCompressedFlags() const
{
    uint8 Result = Super::GetCompressedFlags();

    if (SavedRequestToStartWalking)
    {
        Result |= FLAG_Custom_0;
    }

    if (SavedRequestToStartSprinting)
    {
        Result |= FLAG_Custom_1;
    }

    if (SavedRequestToStartADS)
    {
        Result |= FLAG_Custom_2;
    }

    return Result;
}

bool UGSCharacterMovementComponent::FGSSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
    // Set which moves can be combined together.
    // This will depend on the bit flags that are used.

    if (SavedRequestToStartWalking != ((FGSSavedMove*)NewMove.Get())->SavedRequestToStartWalking)
    {
        return false;
    }

    if (SavedRequestToStartSprinting != ((FGSSavedMove*)NewMove.Get())->SavedRequestToStartSprinting)
    {
        return false;
    }

    if (SavedRequestToStartADS != ((FGSSavedMove*)NewMove.Get())->SavedRequestToStartADS)
    {
        return false;
    }

    return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void UGSCharacterMovementComponent::FGSSavedMove::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
    Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

    UGSCharacterMovementComponent* CharacterMovement = Cast<UGSCharacterMovementComponent>(Character->GetCharacterMovement());
    if (CharacterMovement)
    {
        SavedRequestToStartWalking = CharacterMovement->RequestToStartWalking;
        SavedRequestToStartSprinting = CharacterMovement->RequestToStartSprinting;
        SavedRequestToStartADS = CharacterMovement->RequestToStartADS;
    }
}

void UGSCharacterMovementComponent::FGSSavedMove::PrepMoveFor(ACharacter* Character)
{
    Super::PrepMoveFor(Character);

    UGSCharacterMovementComponent* CharacterMovement = Cast<UGSCharacterMovementComponent>(Character->GetCharacterMovement());

    if (CharacterMovement)
    {
    }
}

UGSCharacterMovementComponent::FGSNetworkPredictionData_Client::FGSNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement)
    : Super(ClientMovement)
{
}

FSavedMovePtr UGSCharacterMovementComponent::FGSNetworkPredictionData_Client::AllocateNewMove()
{
    return FSavedMovePtr(new FGSSavedMove());
}

/** === UGSCharacterMovementComponent === */

UGSCharacterMovementComponent::UGSCharacterMovementComponent()
{
    SprintSpeedMultiplier = 1.4f;
    ADSSpeedMultiplier = 0.8f;
    KnockedDownSpeedMultiplier = 0.4f;

    KnockedDownTag = FGameplayTag::RequestGameplayTag("State.KnockedDown");
    InteractingTag = FGameplayTag::RequestGameplayTag("State.Interacting");
    InteractingRemovalTag = FGameplayTag::RequestGameplayTag("State.InteractingRemoval");
}

float UGSCharacterMovementComponent::GetMaxSpeed() const
{
    AGSCharacterBase* Owner = Cast<AGSHeroCharacter>(GetOwner());

    if (!Owner)
    {
        UE_LOG(LogTemp, Error, TEXT("%s() No Owner"), *FString(__FUNCTION__));
        return Super::GetMaxSpeed();
    }

    if (!Owner->IsAlive())
    {
        return 0.0f;
    }

    UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();

    // Don't move while interacting or being interacted on (revived)
    if (ASC && ASC->GetTagCount(InteractingTag) > ASC->GetTagCount(InteractingRemovalTag))
    {
        return 0.0f;
    }

    if (ASC && ASC->HasMatchingGameplayTag(KnockedDownTag))
    {
        return Owner->GetMoveSpeed() * KnockedDownSpeedMultiplier;
    }

    if (RequestToStartWalking)
    {
        return CurrentMovementSettings.GetSpeedForGait(EALSGait::Walking);
    }

    if (RequestToStartSprinting)
    {
        //return Owner->GetMoveSpeed() * SprintSpeedMultiplier;
        return CurrentMovementSettings.GetSpeedForGait(EALSGait::Sprinting);
    }

    //if (RequestToStartADS)
    //{
    //  return Owner->GetMoveSpeed() * ADSSpeedMultiplier;
    //}

    //return Owner->GetMoveSpeed();
    return CurrentMovementSettings.GetSpeedForGait(EALSGait::Running);
}

float UGSCharacterMovementComponent::GetMaxAcceleration() const
{
    // Update the Acceleration using the Movement Curve.
    // This allows for fine control over movement behavior at each speed.
    if (!IsMovingOnGround() || !CurrentMovementSettings.MovementCurve)
    {
        return Super::GetMaxAcceleration();
    }
    return CurrentMovementSettings.MovementCurve->GetVectorValue(GetMappedSpeed()).X;
}

float UGSCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
    // Update the Deceleration using the Movement Curve.
    // This allows for fine control over movement behavior at each speed.
    if (!IsMovingOnGround() || !CurrentMovementSettings.MovementCurve)
    {
        return Super::GetMaxBrakingDeceleration();
    }
    return CurrentMovementSettings.MovementCurve->GetVectorValue(GetMappedSpeed()).Y;
}

void UGSCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    // The Flags parameter contains the compressed input flags
    // that are stored in the saved move.
    // 
    // UpdateFromCompressed flags simply copies the flags
    // from the saved move into the movement component.
    // 
    // It basically just resets the movement component to the state
    // when the move was made so it can simulate from there.

    RequestToStartWalking = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;

    RequestToStartSprinting = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;

    RequestToStartADS = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
}

FNetworkPredictionData_Client* UGSCharacterMovementComponent::GetPredictionData_Client() const
{
    check(PawnOwner != NULL);

    if (!ClientPredictionData)
    {
        UGSCharacterMovementComponent* MutableThis = const_cast<UGSCharacterMovementComponent*>(this);

        MutableThis->ClientPredictionData = new FGSNetworkPredictionData_Client(*this);
        MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
        MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
    }

    return ClientPredictionData;
}

void UGSCharacterMovementComponent::StartWalking()
{
    RequestToStartWalking = true;
}

void UGSCharacterMovementComponent::StopWalking()
{
    RequestToStartWalking = false;
}

void UGSCharacterMovementComponent::StartSprinting()
{
    RequestToStartSprinting = true;
}

void UGSCharacterMovementComponent::StopSprinting()
{
    RequestToStartSprinting = false;
}

void UGSCharacterMovementComponent::StartAimDownSights()
{
    RequestToStartADS = true;
}

void UGSCharacterMovementComponent::StopAimDownSights()
{
    RequestToStartADS = false;
}

/** ALS */

float UGSCharacterMovementComponent::GetMappedSpeed() const
{
    // Map the character's current speed to the configured movement speeds
    // with a range of 0-3:
    // 0 = stopped,
    // 1 = the Walk Speed,
    // 2 = the Run Speed,
    // 3 = the Sprint Speed.
    //
    // This allows us to vary the movement speeds but still use
    // the mapped range in calculations for consistent results

    const float Speed = Velocity.Size2D();
    const float LocWalkSpeed = CurrentMovementSettings.WalkSpeed;
    const float LocRunSpeed = CurrentMovementSettings.RunSpeed;
    const float LocSprintSpeed = CurrentMovementSettings.SprintSpeed;

    if (Speed > LocRunSpeed)
    {
        return FMath::GetMappedRangeValueClamped(
            {LocRunSpeed, LocSprintSpeed},
            {2.0f, 3.0f},
            Speed
            );
    }

    if (Speed > LocWalkSpeed)
    {
        return FMath::GetMappedRangeValueClamped(
            {LocWalkSpeed, LocRunSpeed},
            {1.0f, 2.0f},
            Speed
            );
    }

    return FMath::GetMappedRangeValueClamped(
        {0.0f, LocWalkSpeed},
        {0.0f, 1.0f},
        Speed
        );
}


void UGSCharacterMovementComponent::SetMovementSettings(FALSMovementSettings NewMovementSettings)
{
    // Set the current movement settings from the owner
    CurrentMovementSettings = NewMovementSettings;
}

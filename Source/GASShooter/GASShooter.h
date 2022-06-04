// Copyright 2020 Dan Kestranek.

#pragma once

#include "CoreMinimal.h"

#define ACTOR_ROLE_FSTRING *(FindObject<UEnum>(ANY_PACKAGE, TEXT("ENetRole"), true)->GetNameStringByValue(GetLocalRole()))
#define GET_ACTOR_ROLE_FSTRING(Actor) *(FindObject<UEnum>(ANY_PACKAGE, TEXT("ENetRole"), true)->GetNameStringByValue(Actor->GetLocalRole()))

#define COLLISION_ABILITY						ECollisionChannel::ECC_GameTraceChannel3
#define COLLISION_PROJECTILE					ECollisionChannel::ECC_GameTraceChannel4
#define COLLISION_ABILITYOVERLAPPROJECTILE		ECollisionChannel::ECC_GameTraceChannel5
#define COLLISION_PICKUP						ECollisionChannel::ECC_GameTraceChannel6
#define COLLISION_INTERACTABLE					ECollisionChannel::ECC_GameTraceChannel7
#define COLLISION_TRACE_WEAPON                  ECollisionChannel::ECC_GameTraceChannel8
#define COLLISION_TRACE_PAWN                    ECollisionChannel::ECC_GameTraceChannel9

UENUM(BlueprintType)
enum class EGSAbilityInputID : uint8
{
	// 0 None
	None				UMETA(DisplayName = "None"),
	// 1 Confirm
	Confirm				UMETA(DisplayName = "Confirm"),
	// 2 Cancel
	Cancel				UMETA(DisplayName = "Cancel"),
	// 3 Walk
	Walk			    UMETA(DisplayName = "Walk"),
	// 4 Sprint
	Sprint				UMETA(DisplayName = "Sprint"),
	// 5 Jump
	Jump				UMETA(DisplayName = "Jump"),
	// 6 PrimaryFire
	PrimaryFire			UMETA(DisplayName = "Primary Fire"),
	// 7 SecondaryFire
	SecondaryFire		UMETA(DisplayName = "Secondary Fire"),
	// 8 Alternate Fire
	AlternateFire		UMETA(DisplayName = "Alternate Fire"),
	// 9 Reload
	Reload				UMETA(DisplayName = "Reload"),
	// 10 NextWeapon
	NextWeapon			UMETA(DisplayName = "Next Weapon"), 
	// 11 PrevWeapon
	PrevWeapon			UMETA(DisplayName = "Previous Weapon"),
	// 12 Interact
	Interact			UMETA(DisplayName = "Interact")
};

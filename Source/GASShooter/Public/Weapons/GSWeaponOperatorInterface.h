// Copyright 2021 Nuraga Wiswakarma.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSWeaponOperatorInterface.generated.h"

class AGSWeapon;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UGSWeaponOperatorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for Actors that is able to operate weapons.
 */
class GASSHOOTER_API IGSWeaponOperatorInterface
{
	GENERATED_BODY()

    // Add interface functions to this class.
    // This is the class that will be inherited to implement this interface.

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	AGSWeapon* GetCurrentWeapon() const;
	virtual AGSWeapon* GetCurrentWeapon_Implementation() const
    {
        return nullptr;
    }

	// Adds a new weapon to the inventory.
	// Returns false if the weapon already exists in the inventory, true if it's a new weapon.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	bool AddWeaponToInventory(AGSWeapon* NewWeapon, bool bEquipWeapon = false);
	virtual bool AddWeaponToInventory_Implementation(AGSWeapon* NewWeapon, bool bEquipWeapon = false)
    {
        return false;
    }

	// Removes a weapon from the inventory.
	// Returns true if the weapon exists and was removed. False if the weapon didn't exist in the inventory.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	bool RemoveWeaponFromInventory(AGSWeapon* WeaponToRemove);
	virtual bool RemoveWeaponFromInventory_Implementation(AGSWeapon* WeaponToRemove)
    {
        return false;
    }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	void RemoveAllWeaponsFromInventory();
	virtual void RemoveAllWeaponsFromInventory_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	void EquipWeapon(AGSWeapon* NewWeapon);
	virtual void EquipWeapon_Implementation(AGSWeapon* NewWeapon) {}

	//UFUNCTION(Server, Reliable)
	//void ServerEquipWeapon(AGSWeapon* NewWeapon);
	//void ServerEquipWeapon_Implementation(AGSWeapon* NewWeapon);
	//bool ServerEquipWeapon_Validate(AGSWeapon* NewWeapon);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	void NextWeapon();
	virtual void NextWeapon_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	void PreviousWeapon();
	virtual void PreviousWeapon_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	int32 GetPrimaryClipAmmo() const;
	virtual int32 GetPrimaryClipAmmo_Implementation() const { return 0; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	int32 GetMaxPrimaryClipAmmo() const;
	virtual int32 GetMaxPrimaryClipAmmo_Implementation() const { return 0; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	int32 GetPrimaryReserveAmmo() const;
	virtual int32 GetPrimaryReserveAmmo_Implementation() const { return 0; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	int32 GetSecondaryClipAmmo() const;
	virtual int32 GetSecondaryClipAmmo_Implementation() const { return 0; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	int32 GetMaxSecondaryClipAmmo() const;
	virtual int32 GetMaxSecondaryClipAmmo_Implementation() const { return 0; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	int32 GetSecondaryReserveAmmo() const;
	virtual int32 GetSecondaryReserveAmmo_Implementation() const { return 0; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GASShooter|Inventory")
	int32 GetNumWeapons() const;
	virtual int32 GetNumWeapons_Implementation() const { return 0; }

	virtual FName GetWeaponAttachPoint() { return FName(); }
};

/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */

#include <algorithm>
#include <cassert>

#include "game-server/inventory.hpp"
#include "game-server/itemmanager.hpp"

int Inventory::getItem(int slot)
{
    for (std::vector< InventoryItem >::iterator i = poss.inventory.begin(),
         i_end = poss.inventory.end(); i != i_end; ++i)
    {
        if (slot == 0)
        {
            return i->itemId;
        }

        slot -= i->itemId ? 1 : i->amount;

        if (slot < 0)
        {
            return 0;
        }
    }
    return 0;
}

int Inventory::getIndex(int slot)
{
    int index = 0;
    for (std::vector< InventoryItem >::iterator i = poss.inventory.begin(),
         i_end = poss.inventory.end(); i != i_end; ++i, ++index)
    {
        if (slot == 0)
        {
            return index;
        }

        slot -= i->itemId ? 1 : i->amount;

        if (slot < 0)
        {
            return -1;
        }
    }
    return -1;
}

int Inventory::fillFreeSlot(int itemId, int amount, int maxPerSlot)
{
    int slot = 0;
    for (int i = 0, i_end = poss.inventory.size(); i < i_end; ++i)
    {
        InventoryItem &it = poss.inventory[i];
        if (it.itemId == 0)
        {
            int nb = std::min(amount, maxPerSlot);
            if (it.amount <= 1)
            {
                it.itemId = itemId;
                it.amount = nb;
            }
            else
            {
                --it.amount;
                InventoryItem iu = { itemId, nb };
                poss.inventory.insert(poss.inventory.begin() + i, iu);
                ++i_end;
            }

            // TODO: fill msg
            amount -= nb;
            if (amount == 0)
            {
                return 0;
            }
        }
        ++slot;
    }

    while (slot < INVENTORY_SLOTS - 1 && amount > 0)
    {
        int nb = std::min(amount, maxPerSlot);
        amount -= nb;
        InventoryItem it = { itemId, nb };
        poss.inventory.push_back(it);
        ++slot;
        // TODO: fill msg
    }

    return amount;
}

int Inventory::insert(int itemId, int amount)
{
    int maxPerSlot = itemManager->getItem(itemId)->getMaxPerSlot();

    for (std::vector< InventoryItem >::iterator i = poss.inventory.begin(),
         i_end = poss.inventory.end(); i != i_end; ++i)
    {
        if (i->itemId == itemId)
        {
            int nb = std::min(maxPerSlot - i->amount, amount);
            i->amount += nb;
            amount -= nb;
            // TODO: fill msg
            if (amount == 0)
            {
                return 0;
            }
        }
    }

    return amount > 0 ? fillFreeSlot(itemId, amount, maxPerSlot) : 0;
}

void Inventory::freeIndex(int i)
{
    InventoryItem &it = poss.inventory[i];

    if (i == (int)poss.inventory.size() - 1)
    {
        poss.inventory.pop_back();
    }
    else if (poss.inventory[i + 1].itemId == 0)
    {
        it.itemId = 0;
        it.amount = poss.inventory[i + 1].amount + 1;
        poss.inventory.erase(poss.inventory.begin() + i + 1);
    }
    else
    {
        it.itemId = 0;
        it.amount = 1;
    }

    if (i > 0 && poss.inventory[i - 1].itemId == 0)
    {
        // Note: "it" is no longer a valid iterator.
        poss.inventory[i - 1].amount += poss.inventory[i].amount;
        poss.inventory.erase(poss.inventory.begin() + i);
    }
}

int Inventory::remove(int itemId, int amount)
{
    for (int i = poss.inventory.size() - 1; i >= 0; --i)
    {
        InventoryItem &it = poss.inventory[i];
        if (it.itemId == itemId)
        {
            int nb = std::min((int)it.amount, amount);
            it.amount -= nb;
            amount -= nb;
            // TODO: fill msg

            // If the slot is empty, compress the inventory.
            if (it.amount == 0)
            {
                freeIndex(i);
            }

            if (amount == 0)
            {
                return 0;
            }
        }
    }

    return amount;
}

bool Inventory::equip(int slot)
{
    int itemId = getItem(slot);
    if (!itemId)
    {
        return false;
    }

    int availableSlots = 0, firstSlot = 0, secondSlot = 0;

    switch (itemManager->getItem(itemId)->getType())
    {
        case ITEM_EQUIPMENT_TWO_HANDS_WEAPON:
            // Special case 1, the two one-handed weapons are to be placed back
            // in the inventory, if there are any.
            if (int id = poss.equipment[EQUIP_FIGHT1_SLOT])
            {
                // Slot 1 full
                if (!insert(id, 1))
                {
                    return false;
                }
            }
            if (int id = poss.equipment[EQUIP_FIGHT2_SLOT])
            {
                // Slot 2 full
                if (!insert(id, 1))
                {
                    return false;
                }
            }

            poss.equipment[EQUIP_FIGHT1_SLOT] = itemId;
            poss.equipment[EQUIP_FIGHT2_SLOT] = 0;
            freeIndex(getIndex(slot));
            return true;

        case ITEM_EQUIPMENT_PROJECTILE:
            poss.equipment[EQUIP_PROJECTILE_SLOT] = itemId;
            return true;

        case ITEM_EQUIPMENT_ONE_HAND_WEAPON:
        case ITEM_EQUIPMENT_SHIELD:
            availableSlots = 2;
            firstSlot = EQUIP_FIGHT1_SLOT;
            secondSlot = EQUIP_FIGHT2_SLOT;
        break;
        case ITEM_EQUIPMENT_RING:
            availableSlots = 2;
            firstSlot = EQUIP_RING1_SLOT;
            secondSlot = EQUIP_RING2_SLOT;
        break;
        case ITEM_EQUIPMENT_BREST:
            availableSlots = 1;
            firstSlot = EQUIP_BREST_SLOT;
        break;
        case ITEM_EQUIPMENT_ARMS:
            availableSlots = 1;
            firstSlot = EQUIP_ARMS_SLOT;
        break;
        case ITEM_EQUIPMENT_HEAD:
            availableSlots = 1;
            firstSlot = EQUIP_HEAD_SLOT;
        break;
        case ITEM_EQUIPMENT_LEGS:
            availableSlots = 1;
            firstSlot = EQUIP_LEGS_SLOT;
        break;
        case ITEM_EQUIPMENT_NECKLACE:
            availableSlots = 1;
            firstSlot = EQUIP_NECKLACE_SLOT;
        break;
        case ITEM_EQUIPMENT_FEET:
            availableSlots = 1;
            firstSlot = EQUIP_FEET_SLOT;
        break;

        case ITEM_UNUSABLE:
        case ITEM_USABLE:
        default:
            return false;
    }

    int id = poss.equipment[firstSlot];

    switch (availableSlots)
    {
    case 2:
        if (id && !poss.equipment[secondSlot] &&
            itemManager->getItem(id)->getType() !=
                ITEM_EQUIPMENT_TWO_HANDS_WEAPON)
        {
            // The first slot is full and the second slot is empty.
            poss.equipment[secondSlot] = itemId;
            freeIndex(getIndex(slot));
            return true;
        }
        // no break!

    case 1:
        if (id && !insert(id, 1))
        {
            return false;
        }
        poss.equipment[firstSlot] = itemId;
        freeIndex(getIndex(slot));
        return true;

    default:
        return false;
    } 
}

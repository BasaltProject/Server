package org.basaltmc.bukkit.inv;

import org.bukkit.event.inventory.InventoryType;

/** How an inventory type is named by the server's windows, and what a slot of each is for. */
public final class InventoryTypes {
    private InventoryTypes() {}

    /** The window kind the server opens for a type, or null for a type that has no window of its own. */
    public static String kindOf(InventoryType type) {
        switch (type) {
            case CHEST, ENDER_CHEST:
                return "chest";
            case DISPENSER:
                return "dispenser";
            case DROPPER:
                return "dropper";
            case FURNACE:
                return "furnace";
            case BLAST_FURNACE:
                return "blast_furnace";
            case SMOKER:
                return "smoker";
            case WORKBENCH:
                return "crafting";
            case ENCHANTING:
                return "enchanting";
            case BREWING:
                return "brewing_stand";
            case MERCHANT:
                return "merchant";
            case ANVIL:
                return "anvil";
            case SMITHING, SMITHING_NEW:
                return "smithing";
            case BEACON:
                return "beacon";
            case HOPPER:
                return "hopper";
            case SHULKER_BOX:
                return "shulker_box";
            case BARREL:
                return "barrel";
            case LECTERN:
                return "lectern";
            case LOOM:
                return "loom";
            case CARTOGRAPHY:
                return "cartography_table";
            case GRINDSTONE:
                return "grindstone";
            case STONECUTTER:
                return "stonecutter";
            case CRAFTER:
                return "crafter";
            default:
                return null;
        }
    }

    public static InventoryType typeOf(String kind) {
        switch (kind) {
            case "player":
                return InventoryType.CRAFTING;
            case "chest", "trapped_chest":
                return InventoryType.CHEST;
            case "ender_chest":
                return InventoryType.ENDER_CHEST;
            case "dispenser":
                return InventoryType.DISPENSER;
            case "dropper":
                return InventoryType.DROPPER;
            case "furnace":
                return InventoryType.FURNACE;
            case "blast_furnace":
                return InventoryType.BLAST_FURNACE;
            case "smoker":
                return InventoryType.SMOKER;
            case "crafting", "crafting_table":
                return InventoryType.WORKBENCH;
            case "enchanting", "enchanting_table":
                return InventoryType.ENCHANTING;
            case "brewing_stand":
                return InventoryType.BREWING;
            case "merchant":
                return InventoryType.MERCHANT;
            case "anvil", "chipped_anvil", "damaged_anvil":
                return InventoryType.ANVIL;
            case "smithing", "smithing_table":
                return InventoryType.SMITHING;
            case "beacon":
                return InventoryType.BEACON;
            case "hopper":
                return InventoryType.HOPPER;
            case "shulker_box":
                return InventoryType.SHULKER_BOX;
            case "barrel":
                return InventoryType.BARREL;
            case "lectern":
                return InventoryType.LECTERN;
            case "loom":
                return InventoryType.LOOM;
            case "cartography_table":
                return InventoryType.CARTOGRAPHY;
            case "grindstone":
                return InventoryType.GRINDSTONE;
            case "stonecutter":
                return InventoryType.STONECUTTER;
            case "crafter":
                return InventoryType.CRAFTER;
            default:
                return InventoryType.CHEST;
        }
    }

    public static InventoryType.SlotType topSlotType(InventoryType type, int slot) {
        switch (type) {
            case CRAFTING, WORKBENCH, CRAFTER:
                return type == InventoryType.CRAFTER ? InventoryType.SlotType.CRAFTING
                    : slot == 0 ? InventoryType.SlotType.RESULT : InventoryType.SlotType.CRAFTING;
            case FURNACE, BLAST_FURNACE, SMOKER:
                return slot == 0 ? InventoryType.SlotType.CRAFTING
                    : slot == 1 ? InventoryType.SlotType.FUEL : InventoryType.SlotType.RESULT;
            case BREWING:
                return slot == 4 ? InventoryType.SlotType.FUEL : InventoryType.SlotType.CRAFTING;
            case ANVIL, GRINDSTONE, CARTOGRAPHY:
                return slot == 2 ? InventoryType.SlotType.RESULT : InventoryType.SlotType.CRAFTING;
            case SMITHING, SMITHING_NEW, LOOM:
                return slot == 3 ? InventoryType.SlotType.RESULT : InventoryType.SlotType.CRAFTING;
            case STONECUTTER:
                return slot == 1 ? InventoryType.SlotType.RESULT : InventoryType.SlotType.CRAFTING;
            case MERCHANT:
                return slot == 2 ? InventoryType.SlotType.RESULT : InventoryType.SlotType.CRAFTING;
            case ENCHANTING, BEACON, LECTERN:
                return InventoryType.SlotType.CRAFTING;
            default:
                return InventoryType.SlotType.CONTAINER;
        }
    }
}

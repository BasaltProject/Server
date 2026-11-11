// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot.packs;
import net.minecraft.data.loot.LootTableSubProvider;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.equipment.trim.ArmorTrim;
import net.minecraft.world.level.storage.loot.LootTable;
public class VanillaEquipmentLoot implements LootTableSubProvider {
    public VanillaEquipmentLoot(LootTableSubProvider.Context output) {
        
    }
    public void run() {
    }
    public LootTable.Builder trialChamberEquipment(Item helmet, Item chestplate, ArmorTrim trim) {
        return LootTable.lootTable();
    }
}

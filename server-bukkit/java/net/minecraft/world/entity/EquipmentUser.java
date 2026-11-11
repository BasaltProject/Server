// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.List;
import java.util.Map;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootParams;
import net.minecraft.world.level.storage.loot.LootTable;
public interface EquipmentUser {
    void setItemSlot(EquipmentSlot slot, ItemStack stack);
    ItemStack getItemBySlot(EquipmentSlot slot);
    void setDropChance(EquipmentSlot slot, float dropChance);
    default void equip(EquipmentTable equipment, LootParams lootParams) {
        equip(equipment.lootTable(), lootParams, equipment.slotDropChances());
    }
    default void equip(ResourceKey<LootTable> lootTable, LootParams lootParams, Map<EquipmentSlot, Float> dropChances) {
        equip(lootTable, lootParams, 0L, dropChances);
    }
    default void equip(ResourceKey<LootTable> lootTable, LootParams lootParams, long optionalLootTableSeed, Map<EquipmentSlot, Float> dropChances) {
        var table = lootParams.getLevel().getServer().reloadableRegistries().getLootTable(lootTable);
        if (table == net.minecraft.world.level.storage.loot.LootTable.EMPTY) {
            return;
        }
        List<EquipmentSlot> filled = new java.util.ArrayList<>();
        for (ItemStack candidate : table.getRandomItems(lootParams, optionalLootTableSeed)) {
            EquipmentSlot slot = resolveSlot(candidate, filled);
            if (slot == null) {
                continue;
            }
            setItemSlot(slot, slot.limit(candidate));
            Float chance = dropChances.get(slot);
            if (chance != null) {
                setDropChance(slot, chance);
            }
            filled.add(slot);
        }
    }
    default EquipmentSlot resolveSlot(ItemStack toEquip, List<EquipmentSlot> alreadyInsertedIntoSlots) {
        if (toEquip.isEmpty()) {
            return null;
        }
        var equippable = toEquip.get(net.minecraft.core.component.DataComponents.EQUIPPABLE);
        EquipmentSlot target = equippable != null ? equippable.slot() : EquipmentSlot.MAINHAND;
        return alreadyInsertedIntoSlots.contains(target) ? null : target;
    }
}

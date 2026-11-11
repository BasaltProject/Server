// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import com.mojang.serialization.Codec;
import java.util.Map;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.storage.loot.LootTable;
public record EquipmentTable(ResourceKey<LootTable> lootTable, Map<EquipmentSlot, Float> slotDropChances) {
    private static Map<EquipmentSlot, Float> uniformChances(float chance) {
        Map<EquipmentSlot, Float> all = new java.util.EnumMap<>(EquipmentSlot.class);
        for (EquipmentSlot each : EquipmentSlot.values()) {
            all.put(each, chance);
        }
        return all;
    }
    public static final Codec<Map<EquipmentSlot, Float>> DROP_CHANCES_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.EquipmentTable.class, "DROP_CHANCES_CODEC");
    public static final Codec<EquipmentTable> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.EquipmentTable.class, "CODEC");
    public EquipmentTable(ResourceKey<LootTable> lootTable, float dropChance) {
        this(lootTable, uniformChances(dropChance));
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import com.mojang.serialization.Codec;
import java.util.Map;
public record DropChances(Map<EquipmentSlot, Float> byEquipment) {
    private static java.util.Map<EquipmentSlot, Float> allAt(float chance) {
        java.util.Map<EquipmentSlot, Float> all = new java.util.EnumMap<>(EquipmentSlot.class);
        for (EquipmentSlot each : EquipmentSlot.values()) {
            all.put(each, chance);
        }
        return all;
    }
    public static final float DEFAULT_EQUIPMENT_DROP_CHANCE = 0.085F;
    public static final float PRESERVE_ITEM_DROP_CHANCE_THRESHOLD = 1.0F;
    public static final int PRESERVE_ITEM_DROP_CHANCE = 2;
    public static final DropChances DEFAULT = new DropChances(allAt(0.085F));
    public static final Codec<DropChances> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.DropChances.class, "CODEC");
    public DropChances withGuaranteedDrop(EquipmentSlot slot) {
        return withEquipmentChance(slot, 2.0F);
    }
    public DropChances withEquipmentChance(EquipmentSlot slot, float chance) {
        if (chance < 0.0F) {
            throw new IllegalArgumentException("Tried to set invalid equipment chance " + chance + " for " + slot);
        }
        if (byEquipment(slot) == chance) {
            return this;
        }
        java.util.Map<EquipmentSlot, Float> copy = new java.util.EnumMap<>(EquipmentSlot.class);
        for (EquipmentSlot each : EquipmentSlot.values()) {
            copy.put(each, each == slot ? chance : byEquipment(each));
        }
        return new DropChances(copy);
    }
    public float byEquipment(EquipmentSlot slot) {
        return byEquipment.getOrDefault(slot, 0.085F);
    }
    public boolean isPreserved(EquipmentSlot slot) {
        return byEquipment(slot) > 1.0F;
    }
}

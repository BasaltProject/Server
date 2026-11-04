// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.equipment;
import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.EquipmentSlot;
public enum ArmorType implements StringRepresentable {
    HELMET(EquipmentSlot.HEAD, 11), CHESTPLATE(EquipmentSlot.CHEST, 16), LEGGINGS(EquipmentSlot.LEGS, 15), BOOTS(EquipmentSlot.FEET, 13),
    BODY(EquipmentSlot.BODY, 16);

    public static final Codec<ArmorType> CODEC = StringRepresentable.fromEnum(ArmorType::values);
    private final EquipmentSlot slot;
    private final int unitDurability;

    private ArmorType(EquipmentSlot slot, int unitDurability) {
        this.slot = slot;
        this.unitDurability = unitDurability;
    }

    public int getDurability(int multiplier) {
        return unitDurability * multiplier;
    }

    public EquipmentSlot getSlot() {
        return slot;
    }

    public String getName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }

    public String getSerializedName() {
        return getName();
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.function.IntFunction;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.item.ItemStack;

public enum EquipmentSlot implements StringRepresentable {
    MAINHAND(Type.HAND, 0, 0, 0, "mainhand"),
    OFFHAND(Type.HAND, 1, 0, 5, "offhand"),
    FEET(Type.HUMANOID_ARMOR, 0, 1, 1, "feet"),
    LEGS(Type.HUMANOID_ARMOR, 1, 1, 2, "legs"),
    CHEST(Type.HUMANOID_ARMOR, 2, 1, 3, "chest"),
    HEAD(Type.HUMANOID_ARMOR, 3, 1, 4, "head"),
    BODY(Type.ANIMAL_ARMOR, 0, 1, 6, "body"),
    SADDLE(Type.SADDLE, 0, 1, 7, "saddle");

    public static final int NO_COUNT_LIMIT = 0;
    public static final List<EquipmentSlot> VALUES = List.of(values());
    public static final IntFunction<EquipmentSlot> BY_ID = id -> {
        for (EquipmentSlot slot : values()) {
            if (slot.id == id) {
                return slot;
            }
        }
        return MAINHAND;
    };
    public static final StringRepresentable.EnumCodec<EquipmentSlot> CODEC = StringRepresentable.fromEnum(EquipmentSlot::values);
    public static final StreamCodec<ByteBuf, EquipmentSlot> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, slot -> slot.id);

    private final Type type;
    private final int index;
    private final int countLimit;
    private final int id;
    private final String name;

    EquipmentSlot(Type type, int index, int countLimit, int id, String name) {
        this.type = type;
        this.index = index;
        this.countLimit = countLimit;
        this.id = id;
        this.name = name;
    }

    public Type getType() {
        return type;
    }

    public int getIndex() {
        return index;
    }

    public int getIndex(int base) {
        return base + index;
    }

    public ItemStack limit(ItemStack toEquip) {
        return countLimit > 0 ? toEquip.split(countLimit) : toEquip;
    }

    public int getId() {
        return id;
    }

    public int getFilterBit(int offset) {
        return id + offset;
    }

    public String getName() {
        return name;
    }

    public boolean isArmor() {
        return type == Type.HUMANOID_ARMOR || type == Type.ANIMAL_ARMOR;
    }

    @Override
    public String getSerializedName() {
        return name;
    }

    public boolean canIncreaseExperience() {
        return type != Type.SADDLE;
    }

    public static EquipmentSlot byName(String name) {
        for (EquipmentSlot slot : values()) {
            if (slot.name.equals(name)) {
                return slot;
            }
        }
        throw new IllegalArgumentException("Invalid slot '" + name + "'");
    }

    public static enum Type {
        HAND, HUMANOID_ARMOR, ANIMAL_ARMOR, SADDLE;
    }
}

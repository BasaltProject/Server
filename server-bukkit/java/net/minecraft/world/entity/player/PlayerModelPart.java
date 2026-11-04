// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;

import com.mojang.serialization.Codec;
import net.minecraft.network.chat.Component;
import net.minecraft.util.StringRepresentable;

public enum PlayerModelPart implements StringRepresentable {
    CAPE(0, "cape"), JACKET(1, "jacket"), LEFT_SLEEVE(2, "left_sleeve"), RIGHT_SLEEVE(3, "right_sleeve"),
    LEFT_PANTS_LEG(4, "left_pants_leg"), RIGHT_PANTS_LEG(5, "right_pants_leg"), HAT(6, "hat");

    public static final Codec<PlayerModelPart> CODEC = StringRepresentable.fromEnum(PlayerModelPart::values);

    private final int bit;
    private final int mask;
    private final String id;

    PlayerModelPart(int bit, String name) {
        this.bit = bit;
        this.mask = 1 << bit;
        this.id = name;
    }

    public int getMask() {
        return mask;
    }

    public int getBit() {
        return bit;
    }

    public String getId() {
        return id;
    }

    public Component getName() {
        return Component.translatable("options.modelPart." + id);
    }

    @Override
    public String getSerializedName() {
        return id;
    }
}

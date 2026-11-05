// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import io.netty.buffer.ByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;

public enum Difficulty implements StringRepresentable {
    PEACEFUL, EASY, NORMAL, HARD;
    public static final StringRepresentable.EnumCodec<Difficulty> CODEC = StringRepresentable.fromEnum(Difficulty::values);
    public static final StreamCodec<ByteBuf, Difficulty> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(Difficulty.class, "STREAM_CODEC");

    public int getId() {
        return ordinal();
    }

    public Component getDisplayName() {
        return Component.translatable("options.difficulty." + getSerializedName());
    }

    public Component getInfo() {
        return Component.translatable("options.difficulty." + getSerializedName() + ".info");
    }

    public static Difficulty byId(int id) {
        Difficulty[] all = values();
        return all[Math.floorMod(id, all.length)];
    }

    public static Difficulty byName(String name) {
        for (Difficulty candidate : values()) {
            if (candidate.getSerializedName().equals(name)) {
                return candidate;
            }
        }
        return NORMAL;
    }

    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}

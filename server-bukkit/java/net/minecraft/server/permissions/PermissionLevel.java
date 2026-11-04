// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.permissions;

import com.mojang.serialization.Codec;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.StringRepresentable;

public enum PermissionLevel implements StringRepresentable {
    ALL("all", 0),
    MODERATORS("moderators", 1),
    GAMEMASTERS("gamemasters", 2),
    ADMINS("admins", 3),
    OWNERS("owners", 4);

    public static final Codec<PermissionLevel> CODEC = StringRepresentable.fromEnum(PermissionLevel::values);
    public static final Codec<PermissionLevel> INT_CODEC = ExtraCodecs.intRange(0, 4).xmap(PermissionLevel::byId, PermissionLevel::id);
    private final String name;
    private final int id;

    PermissionLevel(String name, int id) {
        this.name = name;
        this.id = id;
    }

    public boolean isEqualOrHigherThan(PermissionLevel other) {
        return id >= other.id;
    }

    public static PermissionLevel byId(int level) {
        for (PermissionLevel candidate : values()) {
            if (candidate.id == level) {
                return candidate;
            }
        }
        return level < 0 ? ALL : OWNERS;
    }

    public int id() {
        return id;
    }

    @Override
    public String getSerializedName() {
        return name;
    }
}

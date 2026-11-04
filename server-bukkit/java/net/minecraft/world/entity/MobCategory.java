// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;

public enum MobCategory implements StringRepresentable {
    MONSTER("monster", "MO", 70, false, false, 128),
    CREATURE("creature", "C", 10, true, true, 128),
    AMBIENT("ambient", "AM", 15, true, false, 128),
    AXOLOTLS("axolotls", "AX", 5, true, false, 128),
    UNDERGROUND_WATER_CREATURE("underground_water_creature", "UWC", 5, true, false, 128),
    WATER_CREATURE("water_creature", "WC", 5, true, false, 128),
    WATER_AMBIENT("water_ambient", "WA", 20, true, false, 64),
    MISC("misc", "MI", -1, true, true, 128);

    public static final Codec<MobCategory> CODEC = StringRepresentable.fromEnum(MobCategory::values);

    private final int max;
    private final boolean isFriendly;
    private final boolean isPersistent;
    private final String name;
    private final String debugAbbreviation;
    private final int despawnDistance;

    MobCategory(String name, String debugAbbreviation, int max, boolean isFriendly, boolean isPersistent, int despawnDistance) {
        this.name = name;
        this.debugAbbreviation = debugAbbreviation;
        this.max = max;
        this.isFriendly = isFriendly;
        this.isPersistent = isPersistent;
        this.despawnDistance = despawnDistance;
    }

    public String getName() {
        return name;
    }

    public String getDebugAbbreviation() {
        return debugAbbreviation;
    }

    @Override
    public String getSerializedName() {
        return name;
    }

    public int getMaxInstancesPerChunk() {
        return max;
    }

    public boolean isFriendly() {
        return isFriendly;
    }

    public boolean isPersistent() {
        return isPersistent;
    }

    public int getDespawnDistance() {
        return despawnDistance;
    }

    public int getNoDespawnDistance() {
        return 32;
    }
}

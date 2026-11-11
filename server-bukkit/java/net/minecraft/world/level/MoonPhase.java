// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;
public enum MoonPhase implements StringRepresentable {
    FULL_MOON("full_moon"), WANING_GIBBOUS("waning_gibbous"), THIRD_QUARTER("third_quarter"), WANING_CRESCENT("waning_crescent"),
    NEW_MOON("new_moon"), WAXING_CRESCENT("waxing_crescent"), FIRST_QUARTER("first_quarter"), WAXING_GIBBOUS("waxing_gibbous");
    public static final Codec<MoonPhase> CODEC = StringRepresentable.fromEnum(MoonPhase::values);
    public static final int COUNT = values().length;
    public static final int PHASE_LENGTH = 24000;
    private final String serialized;
    MoonPhase(String serialized) {
        this.serialized = serialized;
    }
    public int index() {
        return ordinal();
    }
    public int startTick() {
        return ordinal() * PHASE_LENGTH;
    }
    @Override
    public String getSerializedName() {
        return serialized;
    }
}

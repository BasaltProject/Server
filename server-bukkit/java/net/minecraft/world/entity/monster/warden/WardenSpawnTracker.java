// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.warden;
import com.mojang.serialization.Codec;
import java.util.OptionalInt;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
public class WardenSpawnTracker {
    private int ticksSinceLastWarning;
    private int warningLevel;
    private int cooldownTicks;
    public static final Codec<WardenSpawnTracker> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.monster.warden.WardenSpawnTracker.class, "CODEC");
    public static final int MAX_WARNING_LEVEL = 4;
    public WardenSpawnTracker(int ticksSinceLastWarning, int warningLevel, int cooldownTicks) {
        this.ticksSinceLastWarning = ticksSinceLastWarning;
        this.warningLevel = Math.max(0, Math.min(MAX_WARNING_LEVEL, warningLevel));
        this.cooldownTicks = cooldownTicks;
    }
    public WardenSpawnTracker() {
        this(0, 0, 0);
    }
    public void tick() {
        if (ticksSinceLastWarning >= 12000) {
            setWarningLevel(warningLevel - 1);
            ticksSinceLastWarning = 0;
        } else {
            ticksSinceLastWarning++;
        }
        if (cooldownTicks > 0) {
            cooldownTicks--;
        }
    }
    public void reset() {
        ticksSinceLastWarning = 0;
        warningLevel = 0;
        cooldownTicks = 0;
    }
    public static OptionalInt tryWarn(ServerLevel level, BlockPos pos, ServerPlayer triggerPlayer) {
        return OptionalInt.empty();
    }
    public void setWarningLevel(int warningLevel) {
        this.warningLevel = Math.max(0, Math.min(MAX_WARNING_LEVEL, warningLevel));
    }
    public int getWarningLevel() {
        return warningLevel;


    }
}

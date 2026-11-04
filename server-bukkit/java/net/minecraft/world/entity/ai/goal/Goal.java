// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;

public abstract class Goal {
    private final EnumSet<Flag> flags = EnumSet.noneOf(Flag.class);

    public abstract boolean canUse();

    public boolean canContinueToUse() {
        return canUse();
    }

    public boolean isInterruptable() {
        return true;
    }

    public void start() {
    }

    public void stop() {
    }

    public boolean requiresUpdateEveryTick() {
        return false;
    }

    public void tick() {
    }

    public void setFlags(EnumSet<Flag> requiredControlFlags) {
        flags.clear();
        flags.addAll(requiredControlFlags);
    }

    public String toString() {
        return getClass().getSimpleName();
    }

    public EnumSet<Flag> getFlags() {
        return flags;
    }

    protected int adjustedTickDelay(int ticks) {
        return requiresUpdateEveryTick() ? ticks : reducedTickDelay(ticks);
    }

    protected static int reducedTickDelay(int ticks) {
        return Mth.positiveCeilDiv(ticks, 2);
    }

    protected static ServerLevel getServerLevel(Entity entity) {
        return getServerLevel(entity.level());
    }

    protected static ServerLevel getServerLevel(Level level) {
        return (ServerLevel) level;
    }

    public static enum Flag {
        MOVE, LOOK, JUMP, TARGET;
    }
}

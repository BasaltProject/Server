// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state;
import net.minecraft.core.Holder;
import net.minecraft.world.level.block.Block;
public class SolidDebugger {
    public static boolean logAndGet(Holder<Block> block, Reason reason) {
        return reason.ordinal() < Reason.FORCE_SOLID_OFF.ordinal();
    }
    public void dump() {
        return;
    }
    public static void runAndDump(Runnable task) {
        task.run();
    }
    public static enum Reason {
        FORCE_SOLID_ON, LARGE_ENOUGH_COLLISION_SHAPE, HIGH_ENOUGH_COLLISION_SHAPE, FORCE_SOLID_OFF, NULL_CACHE, EMPTY_COLLISION_SHAPE, FALLTHROUGH;
    }
}

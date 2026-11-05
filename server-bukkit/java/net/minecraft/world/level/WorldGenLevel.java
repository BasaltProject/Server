// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.function.Supplier;
import net.minecraft.core.BlockPos;
public interface WorldGenLevel extends ServerLevelAccessor {
    long getSeed();
    default boolean ensureCanWrite(BlockPos pos) {
        return true;
    }
    default void setCurrentlyGenerating(Supplier<String> currentlyGenerating) {

    }
}

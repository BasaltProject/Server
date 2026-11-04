// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.function.Supplier;
import net.minecraft.core.BlockPos;
public interface WorldGenLevel extends ServerLevelAccessor {
    default long getSeed() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.WorldGenLevel#getSeed");
    }
    default boolean ensureCanWrite(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.WorldGenLevel#ensureCanWrite");
    }
    default void setCurrentlyGenerating(Supplier<String> currentlyGenerating) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.WorldGenLevel#setCurrentlyGenerating");
    }
}

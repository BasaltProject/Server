// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import net.minecraft.world.level.BlockGetter;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.lighting.ChunkSkyLightSources;
public interface LightChunk extends BlockGetter {
    default void findBlockLightSources(BiConsumer<BlockPos, BlockState> consumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.LightChunk#findBlockLightSources");
    }
    default ChunkSkyLightSources getSkyLightSources() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.LightChunk#getSkyLightSources");
    }
}

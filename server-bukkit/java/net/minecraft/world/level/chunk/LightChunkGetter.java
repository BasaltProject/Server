// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import net.minecraft.core.SectionPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LightLayer;
public interface LightChunkGetter {
    default LightChunk getChunkForLighting(int x, int z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.LightChunkGetter#getChunkForLighting");
    }
    default void onLightUpdate(LightLayer layer, SectionPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.LightChunkGetter#onLightUpdate");
    }
    default BlockGetter getLevel() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.LightChunkGetter#getLevel");
    }
}

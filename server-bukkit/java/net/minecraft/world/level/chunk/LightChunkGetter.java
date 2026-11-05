// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import net.minecraft.core.SectionPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LightLayer;
public interface LightChunkGetter {
    LightChunk getChunkForLighting(int x, int z);
    void onLightUpdate(LightLayer layer, SectionPos pos);
    BlockGetter getLevel();
}

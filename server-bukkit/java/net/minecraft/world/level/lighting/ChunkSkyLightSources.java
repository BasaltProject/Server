// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;

import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.chunk.ChunkAccess;
public class ChunkSkyLightSources {
    protected ChunkSkyLightSources() {
    }
    public static final int NEGATIVE_INFINITY = 0;
    public ChunkSkyLightSources(LevelHeightAccessor level) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.ChunkSkyLightSources#<init>");
    }
    public void fillFrom(ChunkAccess chunk) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.ChunkSkyLightSources#fillFrom");
    }
    public boolean update(BlockGetter level, int x, int y, int z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.ChunkSkyLightSources#update");
    }
    public int getLowestSourceY(int x, int z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.ChunkSkyLightSources#getLowestSourceY");
    }
    public int getHighestLowestSourceY() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.ChunkSkyLightSources#getHighestLowestSourceY");
    }
}

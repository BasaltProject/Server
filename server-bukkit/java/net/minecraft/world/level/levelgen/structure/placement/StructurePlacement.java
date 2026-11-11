// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.placement;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
public interface StructurePlacement {
    Codec<StructurePlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.placement.StructurePlacement.class, "CODEC");
    boolean isStructureChunk(ChunkGeneratorStructureState state, int sourceX, int sourceZ);
    default boolean applyAdditionalChunkRestrictions(int sourceX, int sourceZ, long levelSeed) {
        return true;
    }
    default BlockPos getLocatePos(ChunkPos chunkPos) {
        return new BlockPos(chunkPos.getMinBlockX(), 0, chunkPos.getMinBlockZ()).offset(locateOffset());
    }
    default Vec3i locateOffset() {
        return Vec3i.ZERO;
    }
    MapCodec<? extends StructurePlacement> codec();
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.placement;
import net.minecraft.world.level.ChunkPos;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
public class DimensionOriginStructurePlacement implements StructurePlacement {
    public static final DimensionOriginStructurePlacement INSTANCE = new DimensionOriginStructurePlacement();
    public static final MapCodec<DimensionOriginStructurePlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.placement.DimensionOriginStructurePlacement.class, "CODEC");
    private DimensionOriginStructurePlacement() {
        
    }
    public boolean isStructureChunk(ChunkGeneratorStructureState state, int sourceX, int sourceZ) {
        ChunkPos origin = state.getDimensionOrigin();
        return origin.x() == sourceX && origin.z() == sourceZ;
    }
    public MapCodec<DimensionOriginStructurePlacement> codec() {
        return CODEC;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.levelgen.Heightmap;
public class GravityProcessor implements StructureProcessor {
    private final Heightmap.Types heightmap;
    private final int offset;
    public static final MapCodec<GravityProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.GravityProcessor.class, "MAP_CODEC");
    public GravityProcessor(Heightmap.Types heightmap, int offset) {
        this.heightmap = heightmap;
        this.offset = offset;
    }
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        Heightmap.Types used = heightmap;
        if (level instanceof net.minecraft.server.level.ServerLevel) {
            if (heightmap == Heightmap.Types.WORLD_SURFACE_WG) {
                used = Heightmap.Types.WORLD_SURFACE;
            } else if (heightmap == Heightmap.Types.OCEAN_FLOOR_WG) {
                used = Heightmap.Types.OCEAN_FLOOR;
            }
        }
        BlockPos pos = processedBlockInfo.pos();
        int top = level.getHeight(used, pos.getX(), pos.getZ()) + offset;
        return new StructureTemplate.StructureBlockInfo(new BlockPos(pos.getX(), top + templateRelativePos.getY(), pos.getZ()), processedBlockInfo.state(), processedBlockInfo.nbt());
    }
    public MapCodec<GravityProcessor> codec() {
        return MAP_CODEC;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.EmptyBlockGetter;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.world.level.levelgen.heightproviders.HeightProvider;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class NetherFossilStructure extends Structure {
    public static final MapCodec<NetherFossilStructure> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(settingsCodec(i),
        HeightProvider.CODEC.fieldOf("height").forGetter(c -> c.height)).apply(i, NetherFossilStructure::new));
    public final HeightProvider height;
    public NetherFossilStructure(Structure.StructureSettings settings, HeightProvider height) {
        super(settings);
        this.height = height;
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        WorldgenRandom random = context.random();
        int x = context.chunkPos().getMinBlockX() + random.nextInt(16);
        int z = context.chunkPos().getMinBlockZ() + random.nextInt(16);
        int sea = context.chunkGenerator().getSeaLevel();
        int y = height.sample(random, new WorldGenerationContext(context.chunkGenerator(), context.heightAccessor()));
        NoiseColumn column = context.chunkGenerator().getBaseColumn(x, z, context.heightAccessor(), context.randomState());
        BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos(x, y, z);
        while (y > sea) {
            BlockState above = column.getBlock(y);
            y--;
            BlockState under = column.getBlock(y);
            if (above.isAir() && (under.is(Blocks.SOUL_SAND) || under.isFaceSturdy(EmptyBlockGetter.INSTANCE, probe.setY(y), Direction.UP))) {
                break;
            }
        }
        if (y <= sea) {
            return Optional.empty();
        }
        BlockPos position = new BlockPos(x, y, z);
        return Optional.of(new Structure.GenerationStub(position, builder -> NetherFossilPieces.addPieces(context.structureTemplateManager(), builder, random, position)));
    }
    public StructureType<?> type() {
        return StructureType.NETHER_FOSSIL;


    }
}

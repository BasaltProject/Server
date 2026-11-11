// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record EndGatewayFeature(Optional<BlockPos> exit, boolean exact) implements Feature {
    public static final MapCodec<EndGatewayFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.EndGatewayFeature.class, "CODEC");
    public MapCodec<EndGatewayFeature> codec() {
        return CODEC;
    }
    public static EndGatewayFeature knownExit(BlockPos exit, boolean exact) {
        return new EndGatewayFeature(Optional.of(exit), exact);
    }
    public static EndGatewayFeature delayedExitSearch() {
        return new EndGatewayFeature(Optional.empty(), false);
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        for (BlockPos spot : BlockPos.betweenClosed(origin.offset(-1, -2, -1), origin.offset(1, 2, 1))) {
            boolean alignX = spot.getX() == origin.getX();
            boolean alignY = spot.getY() == origin.getY();
            boolean alignZ = spot.getZ() == origin.getZ();
            boolean cap = Math.abs(spot.getY() - origin.getY()) == 2;
            if (alignX && alignY && alignZ) {
                BlockPos fixed = spot.immutable();
                setBlock(level, fixed, Blocks.END_GATEWAY.defaultBlockState());
                exit.ifPresent(target -> {
                    if (level.getBlockEntity(fixed) instanceof net.minecraft.world.level.block.entity.TheEndGatewayBlockEntity gateway) {
                        gateway.setExitPosition(target, exact);
                    }
                });
            } else if (alignY) {
                setBlock(level, spot, Blocks.AIR.defaultBlockState());
            } else if (cap && alignX && alignZ || (alignX || alignZ) && !cap) {
                setBlock(level, spot, Blocks.BEDROCK.defaultBlockState());
            } else {
                setBlock(level, spot, Blocks.AIR.defaultBlockState());
            }
        }
        return true;
    }
}

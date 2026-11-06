// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import com.mojang.serialization.Codec;
import java.util.BitSet;
import java.util.Optional;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ProtoChunk;
import net.minecraft.world.level.chunk.status.ChunkStatus;
public class BelowZeroRetrogen {
    private static final BitSet kEmpty = new BitSet(0);
    private static final java.util.Set<net.minecraft.resources.ResourceKey<net.minecraft.world.level.biome.Biome>> kRetained =
        java.util.Set.of(net.minecraft.world.level.biome.Biomes.LUSH_CAVES, net.minecraft.world.level.biome.Biomes.DRIPSTONE_CAVES,
            net.minecraft.world.level.biome.Biomes.DEEP_DARK);
    public static final Codec<BelowZeroRetrogen> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.BelowZeroRetrogen.class, "CODEC");
    public static final LevelHeightAccessor UPGRADE_HEIGHT_ACCESSOR = LevelHeightAccessor.create(-64, 64);
    private final ChunkStatus targetStatus;
    private final BitSet missingBedrock;

    protected BelowZeroRetrogen() {
        this(null, Optional.empty());
    }

    private BelowZeroRetrogen(ChunkStatus targetStatus, Optional<BitSet> missingBedrock) {
        this.targetStatus = targetStatus;
        this.missingBedrock = missingBedrock.orElse(kEmpty);
    }

    public static void replaceOldBedrock(ProtoChunk chunk) {
        net.minecraft.core.BlockPos.betweenClosed(0, 0, 0, 15, 4, 15).forEach(pos -> {
            if (chunk.getBlockState(pos).is(net.minecraft.world.level.block.Blocks.BEDROCK)) {
                chunk.setBlockState(pos, net.minecraft.world.level.block.Blocks.DEEPSLATE.defaultBlockState());
            }
        });
    }

    public void applyBedrockMask(ProtoChunk chunk) {
        LevelHeightAccessor heights = chunk.getHeightAccessorForGeneration();
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                if (hasBedrockHole(x, z)) {
                    net.minecraft.core.BlockPos.betweenClosed(x, heights.getMinY(), z, x, heights.getMaxY(), z)
                        .forEach(pos -> chunk.setBlockState(pos, net.minecraft.world.level.block.Blocks.AIR.defaultBlockState()));
                }
            }
        }
    }

    public ChunkStatus targetStatus() {
        return targetStatus;
    }

    public boolean hasBedrockHoles() {
        return !missingBedrock.isEmpty();
    }

    public boolean hasBedrockHole(int x, int z) {
        return missingBedrock.get((z & 15) * 16 + (x & 15));
    }

    public static BiomeResolver getBiomeResolver(BiomeResolver biomeResolver, ChunkAccess protoChunk) {
        if (!protoChunk.isUpgrading()) {
            return biomeResolver;
        }
        return (quartX, quartY, quartZ) -> {
            var biome = biomeResolver.getNoiseBiome(quartX, quartY, quartZ);
            return biome.is(kRetained::contains) ? biome : protoChunk.getNoiseBiome(quartX, 0, quartZ);
        };
    }
}

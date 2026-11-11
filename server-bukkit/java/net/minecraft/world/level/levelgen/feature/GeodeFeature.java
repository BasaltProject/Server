// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import com.mojang.datafixers.util.Pair;
import java.util.function.Predicate;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import java.util.List;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.GeodeBlockSettings;
import net.minecraft.world.level.levelgen.GeodeCrackSettings;
import net.minecraft.world.level.levelgen.GeodeLayerSettings;
public record GeodeFeature(GeodeBlockSettings blockSettings, GeodeLayerSettings layerSettings, GeodeCrackSettings crackSettings, double usePotentialPlacementsChance, double useAlternateLayer0Chance, boolean placementsRequireLayer0Alternate, IntProvider outerWallDistance, IntProvider distributionPoints, IntProvider pointOffset, int minGenOffset, int maxGenOffset, double noiseMultiplier, int invalidBlocksThreshold) implements Feature {
    private static final NormalNoise kNoise = NormalNoise.createParity(-4, 1.0D);
    private static final Direction[] kSides = Direction.values();
    public static final Codec<Double> CHANCE_RANGE = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.GeodeFeature.class, "CHANCE_RANGE");
    public static final MapCodec<GeodeFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.GeodeFeature.class, "CODEC");
    public MapCodec<GeodeFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        List<Pair<BlockPos, Integer>> centres = new java.util.LinkedList<>();
        int pointCount = distributionPoints.sample(random);
        Noise noise = kNoise.create(new WorldgenRandom(new LegacyRandomSource(level.getSeed())));
        List<BlockPos> crackCentres = new java.util.LinkedList<>();
        double crackGrowth = (double) pointCount / (double) outerWallDistance.maxInclusive();
        double airLimit = 1.0D / Math.sqrt(layerSettings.filling);
        double innerLimit = 1.0D / Math.sqrt(layerSettings.innerLayer + crackGrowth);
        double middleLimit = 1.0D / Math.sqrt(layerSettings.middleLayer + crackGrowth);
        double outerLimit = 1.0D / Math.sqrt(layerSettings.outerLayer + crackGrowth);
        double crackLimit = 1.0D / Math.sqrt(crackSettings.baseCrackSize + random.nextDouble() / 2.0D + (pointCount > 3 ? crackGrowth : 0.0D));
        boolean cracked = (double) random.nextFloat() < crackSettings.generateCrackChance;
        int bad = 0;
        for (int i = 0; i < pointCount; i++) {
            int px = outerWallDistance.sample(random);
            int py = outerWallDistance.sample(random);
            int pz = outerWallDistance.sample(random);
            BlockPos centre = origin.offset(px, py, pz);
            BlockState at = level.getBlockState(centre);
            if ((at.isAir() || at.is(blockSettings.invalidBlocks())) && ++bad > invalidBlocksThreshold) {
                return false;
            }
            centres.add(Pair.of(centre, pointOffset.sample(random)));
        }
        if (cracked) {
            int pick = random.nextInt(4);
            int shift = pointCount * 2 + 1;
            int[] ys = {7, 5, 1};
            for (int y : ys) {
                if (pick == 0) {
                    crackCentres.add(origin.offset(shift, y, 0));
                } else if (pick == 1) {
                    crackCentres.add(origin.offset(0, y, shift));
                } else if (pick == 2) {
                    crackCentres.add(origin.offset(shift, y, shift));
                } else {
                    crackCentres.add(origin.offset(0, y, 0));
                }
            }
        }
        List<BlockPos> crystalSpots = new java.util.ArrayList<>();
        HolderSet<Block> locked = blockSettings.cannotReplace();
        Predicate<BlockState> replaceable = s -> !s.is(locked);
        for (BlockPos spot : BlockPos.betweenClosed(origin.offset(minGenOffset, minGenOffset, minGenOffset), origin.offset(maxGenOffset, maxGenOffset, maxGenOffset))) {
            double wobble = (double) noise.get((double) spot.getX(), (double) spot.getY(), (double) spot.getZ()) * noiseMultiplier;
            double shell = 0.0D;
            for (Pair<BlockPos, Integer> centre : centres) {
                shell += Mth.invSqrt(spot.distSqr(centre.getFirst()) + (double) centre.getSecond()) + wobble;
            }
            if (shell < outerLimit) {
                continue;
            }
            if (shell >= airLimit) {
                safeSetBlock(level, spot, blockSettings.fillingProvider().value().getState(level, random, spot), replaceable);
                continue;
            }
            double crack = 0.0D;
            for (BlockPos centre : crackCentres) {
                crack += Mth.invSqrt(spot.distSqr(centre) + (double) crackSettings.crackPointOffset) + wobble;
            }
            if (cracked && crack >= crackLimit) {
                safeSetBlock(level, spot, Blocks.AIR.defaultBlockState(), replaceable);
                for (Direction side : kSides) {
                    BlockPos next = spot.relative(side);
                    var fluid = level.getFluidState(next);
                    if (!fluid.isEmpty()) {
                        level.scheduleTick(next, fluid.getType(), 0);
                    }
                }
            } else if (shell >= innerLimit) {
                boolean alt = (double) random.nextFloat() < useAlternateLayer0Chance;
                BlockStateProvider layer = alt ? blockSettings.alternateInnerLayerProvider().value() : blockSettings.innerLayerProvider().value();
                safeSetBlock(level, spot, layer.getState(level, random, spot), replaceable);
                if ((!placementsRequireLayer0Alternate || alt) && (double) random.nextFloat() < usePotentialPlacementsChance) {
                    crystalSpots.add(spot.immutable());
                }
            } else if (shell >= middleLimit) {
                safeSetBlock(level, spot, blockSettings.middleLayerProvider().value().getState(level, random, spot), replaceable);
            } else {
                safeSetBlock(level, spot, blockSettings.outerLayerProvider().value().getState(level, random, spot), replaceable);
            }
        }
        List<BlockState> choices = blockSettings.innerPlacements();
        for (BlockPos spot : crystalSpots) {
            BlockState crystal = choices.get(random.nextInt(choices.size()));
            for (Direction side : kSides) {
                if (crystal.hasProperty(BlockStateProperties.FACING)) {
                    crystal = crystal.setValue(BlockStateProperties.FACING, side);
                }
                BlockPos target = spot.relative(side);
                BlockState there = level.getBlockState(target);
                if (crystal.hasProperty(BlockStateProperties.WATERLOGGED)) {
                    crystal = crystal.setValue(BlockStateProperties.WATERLOGGED, there.getFluidState().isSource());
                }
                if (net.minecraft.world.level.block.BuddingAmethystBlock.canClusterGrowAtState(there)) {
                    safeSetBlock(level, target, crystal, replaceable);
                    break;
                }
            }
        }
        return true;
    }
}

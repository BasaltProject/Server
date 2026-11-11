// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.core.registries.Registries;
import net.minecraft.util.valueproviders.IntProviders;
import net.minecraft.core.Direction;
import com.mojang.serialization.Codec;
import com.mojang.datafixers.util.Function10;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.placement.CaveSurface;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class VegetationPatchFeature implements Feature {
    private boolean layGround(WorldGenLevel level, Predicate<BlockState> replaceable, RandomSource random, BlockPos.MutableBlockPos cursor, int layers) {
        for (int i = 0; i < layers; i++) {
            BlockState wanted = groundState.value().getState(level, random, cursor);
            BlockState current = level.getBlockState(cursor);
            if (wanted.is(current.getBlock())) {
                continue;
            }
            if (!replaceable.test(current)) {
                return i != 0;
            }
            level.setBlock(cursor, wanted, 2);
            cursor.move(surface.getDirection());
        }
        return true;
    }
    public static final MapCodec<VegetationPatchFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.VegetationPatchFeature.class, "CODEC");
    protected final HolderSet<Block> replaceable;
    protected final Holder<BlockStateProvider> groundState;
    protected final Holder<PlacedFeature> vegetationFeature;
    protected final CaveSurface surface;
    protected final IntProvider depth;
    protected final float extraBottomBlockChance;
    protected final int verticalRange;
    protected final float vegetationChance;
    protected final IntProvider xzRadius;
    protected final float extraEdgeColumnChance;
    protected static <T extends VegetationPatchFeature> MapCodec<T> makeCodec(Function10<HolderSet<Block>, Holder<BlockStateProvider>, Holder<PlacedFeature>, CaveSurface, IntProvider, Float, Integer, Float, IntProvider, Float, T> constructor) {
        return com.mojang.serialization.codecs.RecordCodecBuilder.<T>mapCodec(i -> i.group(
            org.basaltmc.bukkit.facade.LazyCodecs.<HolderSet<Block>>codec(VegetationPatchFeature.class, "REPLACEABLE").fieldOf("replaceable").forGetter(p -> p.replaceable),
            BlockStateProvider.CODEC.fieldOf("ground_state").forGetter(p -> p.groundState),
            PlacedFeature.CODEC.fieldOf("vegetation_feature").forGetter(p -> p.vegetationFeature),
            CaveSurface.CODEC.fieldOf("surface").forGetter(p -> p.surface),
            IntProviders.codec(1, 128).fieldOf("depth").forGetter(p -> p.depth),
            Codec.floatRange(0.0F, 1.0F).fieldOf("extra_bottom_block_chance").forGetter(p -> p.extraBottomBlockChance),
            Codec.intRange(1, 256).fieldOf("vertical_range").forGetter(p -> p.verticalRange),
            Codec.floatRange(0.0F, 1.0F).fieldOf("vegetation_chance").forGetter(p -> p.vegetationChance),
            IntProviders.POSITIVE_CODEC.fieldOf("xz_radius").forGetter(p -> p.xzRadius),
            Codec.floatRange(0.0F, 1.0F).fieldOf("extra_edge_column_chance").forGetter(p -> p.extraEdgeColumnChance))
            .apply(i, constructor));
    }
    public VegetationPatchFeature(HolderSet<Block> replaceable, Holder<BlockStateProvider> groundState, Holder<PlacedFeature> vegetationFeature, CaveSurface surface, IntProvider depth, float extraBottomBlockChance, int verticalRange, float vegetationChance, IntProvider xzRadius, float extraEdgeColumnChance) {
        this.replaceable = replaceable;
        this.groundState = groundState;
        this.vegetationFeature = vegetationFeature;
        this.surface = surface;
        this.depth = depth;
        this.extraBottomBlockChance = extraBottomBlockChance;
        this.verticalRange = verticalRange;
        this.vegetationChance = vegetationChance;
        this.xzRadius = xzRadius;
        this.extraEdgeColumnChance = extraEdgeColumnChance;
    }
    public MapCodec<? extends VegetationPatchFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        int rx = xzRadius.sample(random) + 1;
        int rz = xzRadius.sample(random) + 1;
        Set<BlockPos> ground = placeGroundPatch(level, random, origin, s -> s.is(replaceable), rx, rz);
        for (BlockPos spot : ground) {
            if (vegetationChance > 0.0F && random.nextFloat() < vegetationChance) {
                placeVegetation(level, chunkGenerator, random, spot);
            }
        }
        return !ground.isEmpty();
    }
    protected Set<BlockPos> placeGroundPatch(WorldGenLevel level, RandomSource random, BlockPos origin, Predicate<BlockState> replaceable, int xRadius, int zRadius) {
        BlockPos.MutableBlockPos cursor = origin.mutable();
        BlockPos.MutableBlockPos under = cursor.mutable();
        Direction inward = surface.getDirection();
        Direction outward = inward.getOpposite();
        Set<BlockPos> found = new java.util.HashSet<>();
        for (int dx = -xRadius; dx <= xRadius; dx++) {
            boolean edgeX = dx == -xRadius || dx == xRadius;
            for (int dz = -zRadius; dz <= zRadius; dz++) {
                boolean edgeZ = dz == -zRadius || dz == zRadius;
                boolean corner = edgeX && edgeZ;
                boolean side = (edgeX || edgeZ) && !corner;
                if (corner || (side && !(extraEdgeColumnChance != 0.0F && !(random.nextFloat() > extraEdgeColumnChance)))) {
                    continue;
                }
                cursor.setWithOffset(origin, dx, 0, dz);
                for (int n = 0; level.isStateAtPosition(cursor, BlockState::isAir) && n < verticalRange; n++) {
                    cursor.move(inward);
                }
                for (int n = 0; level.isStateAtPosition(cursor, s -> !s.isAir()) && n < verticalRange; n++) {
                    cursor.move(outward);
                }
                under.setWithOffset(cursor, surface.getDirection());
                BlockState underState = level.getBlockState(under);
                if (level.isEmptyBlock(cursor) && underState.isFaceSturdy(level, under, surface.getDirection().getOpposite())) {
                    int layers = depth.sample(random) + (extraBottomBlockChance > 0.0F && random.nextFloat() < extraBottomBlockChance ? 1 : 0);
                    BlockPos top = under.immutable();
                    if (layGround(level, replaceable, random, under, layers)) {
                        found.add(top);
                    }
                }
            }
        }
        return found;
    }
    protected boolean placeVegetation(WorldGenLevel level, ChunkGenerator generator, RandomSource random, BlockPos vegetationPos) {
        return vegetationFeature.value().place(level, generator, random, vegetationPos.relative(surface.getDirection().getOpposite()));
    }
}

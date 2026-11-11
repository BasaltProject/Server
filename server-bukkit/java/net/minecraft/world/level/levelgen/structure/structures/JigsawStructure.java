// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.heightproviders.HeightProvider;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
import net.minecraft.world.level.levelgen.structure.TerrainAdjustment;
import net.minecraft.world.level.levelgen.structure.pools.DimensionPadding;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
import net.minecraft.world.level.levelgen.structure.pools.alias.PoolAliasBinding;
import net.minecraft.world.level.levelgen.structure.templatesystem.LiquidSettings;
public final class JigsawStructure extends Structure {
    private static final int kDefaultReach = 80;
    private final Holder<StructureTemplatePool> startPool;
    private final Optional<Identifier> startJigsawName;
    private final int maxDepth;
    private final HeightProvider startHeight;
    private final boolean useExpansionHack;
    private final Optional<Heightmap.Types> projectStartToHeightmap;
    private final MaxDistance maxDistanceFromCenter;
    private final List<PoolAliasBinding> poolAliases;
    private final DimensionPadding dimensionPadding;
    private final LiquidSettings liquidSettings;
    public static final DimensionPadding DEFAULT_DIMENSION_PADDING = DimensionPadding.ZERO;
    public static final LiquidSettings DEFAULT_LIQUID_SETTINGS = LiquidSettings.APPLY_WATERLOGGING;
    public static final int MAX_TOTAL_STRUCTURE_RANGE = 128;
    public static final int MIN_DEPTH = 0;
    public static final int MAX_DEPTH = 20;
    public static final MapCodec<JigsawStructure> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.<JigsawStructure>mapCodec(i -> i.group(settingsCodec(i),
        StructureTemplatePool.CODEC.fieldOf("start_pool").forGetter(j -> j.startPool), Identifier.CODEC.optionalFieldOf("start_jigsaw_name").forGetter(j -> j.startJigsawName),
        Codec.intRange(MIN_DEPTH, MAX_DEPTH).fieldOf("size").forGetter(j -> j.maxDepth), HeightProvider.CODEC.fieldOf("start_height").forGetter(j -> j.startHeight),
        Codec.BOOL.fieldOf("use_expansion_hack").forGetter(j -> j.useExpansionHack), Heightmap.Types.CODEC.optionalFieldOf("project_start_to_heightmap").forGetter(j -> j.projectStartToHeightmap),
        MaxDistance.CODEC.fieldOf("max_distance_from_center").forGetter(j -> j.maxDistanceFromCenter), Codec.list(PoolAliasBinding.CODEC).optionalFieldOf("pool_aliases", List.of()).forGetter(j -> j.poolAliases),
        DimensionPadding.CODEC.optionalFieldOf("dimension_padding", DEFAULT_DIMENSION_PADDING).forGetter(j -> j.dimensionPadding),
        LiquidSettings.CODEC.optionalFieldOf("liquid_settings", DEFAULT_LIQUID_SETTINGS).forGetter(j -> j.liquidSettings)).apply(i, JigsawStructure::new)).validate(JigsawStructure::verifyRange);
    public JigsawStructure(Structure.StructureSettings settings, Holder<StructureTemplatePool> startPool, Optional<Identifier> startJigsawName, int maxDepth, HeightProvider startHeight, boolean useExpansionHack, Optional<Heightmap.Types> projectStartToHeightmap, MaxDistance maxDistanceFromCenter, List<PoolAliasBinding> poolAliases, DimensionPadding dimensionPadding, LiquidSettings liquidSettings) {
        super(settings);
        this.startPool = startPool;
        this.startJigsawName = startJigsawName;
        this.maxDepth = maxDepth;
        this.startHeight = startHeight;
        this.projectStartToHeightmap = projectStartToHeightmap;
        this.useExpansionHack = useExpansionHack;
        this.poolAliases = poolAliases;
        this.maxDistanceFromCenter = maxDistanceFromCenter;
        this.liquidSettings = liquidSettings;
        this.dimensionPadding = dimensionPadding;
    }
    public JigsawStructure(Structure.StructureSettings settings, Holder<StructureTemplatePool> startPool, int maxDepth, HeightProvider startHeight, boolean useExpansionHack, Heightmap.Types projectStartToHeightmap) {
        this(settings, startPool, Optional.empty(), maxDepth, startHeight, useExpansionHack, Optional.of(projectStartToHeightmap), new MaxDistance(kDefaultReach), List.of(), DEFAULT_DIMENSION_PADDING, DEFAULT_LIQUID_SETTINGS);
    }
    public JigsawStructure(Structure.StructureSettings settings, Holder<StructureTemplatePool> startPool, int maxDepth, HeightProvider startHeight, boolean useExpansionHack) {
        this(settings, startPool, Optional.empty(), maxDepth, startHeight, useExpansionHack, Optional.empty(), new MaxDistance(kDefaultReach), List.of(), DEFAULT_DIMENSION_PADDING, DEFAULT_LIQUID_SETTINGS);
    }
    private static com.mojang.serialization.DataResult<JigsawStructure> verifyRange(JigsawStructure structure) {
        int edge = structure.terrainAdaptation() == TerrainAdjustment.NONE ? 0 : 12;
        return structure.maxDistanceFromCenter.horizontal() + edge > MAX_TOTAL_STRUCTURE_RANGE
            ? com.mojang.serialization.DataResult.error(() -> "Horizontal structure size including terrain adaptation must not exceed 128")
            : com.mojang.serialization.DataResult.success(structure);
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        return Optional.empty();
    }
    public StructureType<?> type() {
        return StructureType.JIGSAW;
    }
    public Holder<StructureTemplatePool> getStartPool() {
        return startPool;
    }
    public List<PoolAliasBinding> getPoolAliases() {
        return poolAliases;
    }
    public static record MaxDistance(int horizontal, int vertical) {
        private static final Codec<Integer> kHorizontal = Codec.intRange(1, 128);
        private static final Codec<MaxDistance> kFull = com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
            kHorizontal.fieldOf("horizontal").forGetter(MaxDistance::horizontal),
            net.minecraft.util.ExtraCodecs.intRange(1, net.minecraft.world.level.dimension.DimensionType.Y_SIZE).optionalFieldOf("vertical",
                net.minecraft.world.level.dimension.DimensionType.Y_SIZE).forGetter(MaxDistance::vertical)).apply(i, MaxDistance::new));
        public static final Codec<MaxDistance> CODEC = Codec.either(kFull, kHorizontal).xmap(
            either -> either.map(java.util.function.Function.identity(), MaxDistance::new),
            distance -> distance.horizontal == distance.vertical ? com.mojang.datafixers.util.Either.right(distance.horizontal) : com.mojang.datafixers.util.Either.left(distance));
        public MaxDistance(int value) {
            this(value, value);


        }
    }
}

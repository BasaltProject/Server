// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.util.Mth;
import net.minecraft.core.QuartPos;
import net.minecraft.core.Holder;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import java.util.ArrayList;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class RuinedPortalStructure extends Structure {
    private final List<Setup> setups;
    private static final int kPortals = 10;
    private static final int kGiantPortals = 3;
    private static boolean roll(WorldgenRandom random, float limit) {
        if (limit == 0.0F) {
            return false;
        }
        return limit == 1.0F || random.nextFloat() < limit;
    }
    private static int within(RandomSource random, int preferredMin, int max) {
        return preferredMin < max ? Mth.randomBetweenInclusive(random, preferredMin, max) : max;
    }
    private static int settle(RandomSource random, ChunkGenerator generator, RuinedPortalPiece.VerticalPlacement placement, boolean airPocket, int surfaceY, int ySpan,
        BoundingBox box, LevelHeightAccessor accessor, RandomState randomState) {
        int floor = accessor.getMinY() + 15;
        int start;
        switch (placement) {
            case IN_NETHER -> {
                if (airPocket) {
                    start = Mth.randomBetweenInclusive(random, 32, 100);
                } else if (random.nextFloat() < 0.5F) {
                    start = Mth.randomBetweenInclusive(random, 27, 29);
                } else {
                    start = Mth.randomBetweenInclusive(random, 29, 100);
                }
            }
            case IN_MOUNTAIN -> start = within(random, 70, surfaceY - ySpan);
            case UNDERGROUND -> start = within(random, floor, surfaceY - ySpan);
            case PARTLY_BURIED -> start = surfaceY - ySpan + Mth.randomBetweenInclusive(random, 2, 8);
            default -> start = surfaceY;
        }
        BlockPos[] corners = {new BlockPos(box.minX(), 0, box.minZ()), new BlockPos(box.maxX(), 0, box.minZ()),
            new BlockPos(box.minX(), 0, box.maxZ()), new BlockPos(box.maxX(), 0, box.maxZ())};
        List<NoiseColumn> columns = new ArrayList<>();
        for (BlockPos corner : corners) {
            columns.add(generator.getBaseColumn(corner.getX(), corner.getZ(), accessor, randomState));
        }
        Heightmap.Types map = placement == RuinedPortalPiece.VerticalPlacement.ON_OCEAN_FLOOR ? Heightmap.Types.OCEAN_FLOOR_WG : Heightmap.Types.WORLD_SURFACE_WG;
        int y = start;
        for (; y > floor; y--) {
            int solid = 0;
            for (NoiseColumn column : columns) {
                BlockState state = column.getBlock(y);
                if (map.isOpaque().test(state) && ++solid == 3) {
                    return y;
                }
            }
        }
        return y;
    }
    public static final MapCodec<RuinedPortalStructure> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(settingsCodec(i),
        net.minecraft.util.ExtraCodecs.nonEmptyList(Setup.CODEC.listOf()).fieldOf("setups").forGetter(s -> s.setups)).apply(i, RuinedPortalStructure::new));
    public RuinedPortalStructure(Structure.StructureSettings settings, List<Setup> setups) {
        super(settings);
        this.setups = setups;
    }
    public RuinedPortalStructure(Structure.StructureSettings settings, Setup setup) {
        this(settings, List.of(setup));
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        WorldgenRandom random = context.random();
        Setup chosen = null;
        if (setups.size() > 1) {
            float total = 0.0F;
            for (Setup each : setups) {
                total += each.weight();
            }
            float pick = random.nextFloat();
            for (Setup each : setups) {
                pick -= each.weight() / total;
                if (pick < 0.0F) {
                    chosen = each;
                    break;
                }
            }
        } else {
            chosen = setups.get(0);
        }
        if (chosen == null) {
            throw new IllegalStateException();
        }
        final Setup setup = chosen;
        boolean airPocket = roll(random, setup.airPocketProbability());
        Identifier id = random.nextFloat() < 0.05F
            ? Identifier.withDefaultNamespace("ruined_portal/giant_portal_" + (random.nextInt(kGiantPortals) + 1))
            : Identifier.withDefaultNamespace("ruined_portal/portal_" + (random.nextInt(kPortals) + 1));
        StructureTemplate template = context.structureTemplateManager().getOrCreate(id);
        Rotation[] turns = Rotation.values();
        Rotation rotation = turns[random.nextInt(turns.length)];
        Mirror mirror = random.nextFloat() < 0.5F ? Mirror.NONE : Mirror.FRONT_BACK;
        BlockPos pivot = new BlockPos(template.getSize().getX() / 2, 0, template.getSize().getZ() / 2);
        ChunkGenerator generator = context.chunkGenerator();
        LevelHeightAccessor accessor = context.heightAccessor();
        RandomState randomState = context.randomState();
        BlockPos base = context.chunkPos().getWorldPosition();
        BoundingBox box = template.getBoundingBox(base, rotation, pivot, mirror);
        BlockPos middle = box.getCenter();
        int surface = generator.getBaseHeight(middle.getX(), middle.getZ(), RuinedPortalPiece.getHeightMapType(setup.placement()), accessor, randomState) - 1;
        int y = settle(random, generator, setup.placement(), airPocket, surface, box.getYSpan(), box, accessor, randomState);
        BlockPos origin = new BlockPos(base.getX(), y, base.getZ());
        return Optional.of(new Structure.GenerationStub(origin, builder -> {
            Holder<Biome> biome = context.biomeResolver().getNoiseBiome(QuartPos.fromBlock(origin.getX()), QuartPos.fromBlock(origin.getY()),
                QuartPos.fromBlock(origin.getZ()));
            boolean cold = setup.canBeCold() && biome.value().coldEnoughToSnow(origin, generator.getSeaLevel());
            RuinedPortalPiece.Properties props = new RuinedPortalPiece.Properties(cold, setup.mossiness(), airPocket, setup.overgrown(), setup.vines(), setup.replaceWithBlackstone());
            builder.addPiece(new RuinedPortalPiece(context.registryAccess(), context.structureTemplateManager(), origin, setup.placement(), props, id, template, rotation, mirror, pivot));
        }));
    }
    public StructureType<?> type() {
        return StructureType.RUINED_PORTAL;


    }
    public static record Setup(RuinedPortalPiece.VerticalPlacement placement, float airPocketProbability, float mossiness, boolean overgrown, boolean vines, boolean canBeCold, boolean replaceWithBlackstone, float weight) {
        public static final Codec<Setup> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(RuinedPortalPiece.VerticalPlacement.CODEC.fieldOf("placement").forGetter(Setup::placement),
            Codec.floatRange(0.0F, 1.0F).fieldOf("air_pocket_probability").forGetter(Setup::airPocketProbability), Codec.floatRange(0.0F, 1.0F).fieldOf("mossiness").forGetter(Setup::mossiness),
            Codec.BOOL.fieldOf("overgrown").forGetter(Setup::overgrown), Codec.BOOL.fieldOf("vines").forGetter(Setup::vines), Codec.BOOL.fieldOf("can_be_cold").forGetter(Setup::canBeCold),
            Codec.BOOL.fieldOf("replace_with_blackstone").forGetter(Setup::replaceWithBlackstone), net.minecraft.util.ExtraCodecs.POSITIVE_FLOAT.fieldOf("weight").forGetter(Setup::weight)).apply(i, Setup::new));
    }
}

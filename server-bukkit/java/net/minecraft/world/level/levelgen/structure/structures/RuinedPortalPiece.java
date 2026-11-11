// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.templatesystem.*;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.tags.BlockTags;
import org.basaltmc.bukkit.facade.StateProps;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import com.mojang.serialization.Codec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.TemplateStructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public class RuinedPortalPiece extends TemplateStructurePiece {
    private VerticalPlacement verticalPlacement;
    private Properties properties;
    private static final float[] kSpread = {1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 0.9F, 0.9F, 0.8F, 0.7F, 0.6F, 0.4F, 0.2F};
    private static StructurePlaceSettings settingsFor(HolderLookup.Provider registries, Mirror mirror, Rotation rotation, VerticalPlacement placement, BlockPos pivot, Properties props) {
        BlockIgnoreProcessor ignore = props.airPocket() ? BlockIgnoreProcessor.STRUCTURE_BLOCK : BlockIgnoreProcessor.STRUCTURE_AND_AIR;
        List<ProcessorRule> rules = new ArrayList<>();
        rules.add(swap(Blocks.GOLD_BLOCK, 0.3F, Blocks.AIR));
        if (placement == VerticalPlacement.ON_OCEAN_FLOOR) {
            rules.add(swap(Blocks.LAVA, Blocks.MAGMA_BLOCK));
        } else {
            rules.add(props.cold() ? swap(Blocks.LAVA, Blocks.NETHERRACK) : swap(Blocks.LAVA, 0.2F, Blocks.MAGMA_BLOCK));
        }
        if (!props.cold()) {
            rules.add(swap(Blocks.NETHERRACK, 0.07F, Blocks.MAGMA_BLOCK));
        }
        StructurePlaceSettings settings = new StructurePlaceSettings().setRotation(rotation).setMirror(mirror).setRotationPivot(pivot)
            .addProcessor(ignore).addProcessor(new RuleProcessor(rules)).addProcessor(new BlockAgeProcessor(props.mossiness()))
            .addProcessor(new ProtectedBlockProcessor(registries.lookupOrThrow(Registries.BLOCK).getOrThrow(BlockTags.FEATURES_CANNOT_REPLACE)))
            .addProcessor(new LavaSubmergedBlockProcessor());
        if (props.replaceWithBlackstone()) {
            settings.addProcessor(BlackstoneReplaceProcessor.INSTANCE);
        }
        return settings;
    }
    private static StructurePlaceSettings settingsFromTag(StructurePieceSerializationContext context, CompoundTag tag, Identifier location) {
        StructureTemplate template = context.structureTemplateManager().getOrCreate(location);
        var size = template.getSize();
        BlockPos pivot = new BlockPos(size.getX() / 2, 0, size.getZ() / 2);
        return settingsFor(context.registryAccess(), Mirror.valueOf(tag.getStringOr("Mirror", "NONE")), Rotation.valueOf(tag.getStringOr("Rotation", "NONE")),
            readPlacement(tag), pivot, readProperties(tag));
    }
    private static VerticalPlacement readPlacement(CompoundTag tag) {
        String wanted = tag.getStringOr("VerticalPlacement", "on_land_surface");
        for (VerticalPlacement each : VerticalPlacement.values()) {
            if (each.getSerializedName().equals(wanted)) {
                return each;
            }
        }
        return VerticalPlacement.ON_LAND_SURFACE;
    }
    private static Properties readProperties(CompoundTag tag) {
        CompoundTag p = tag.getCompoundOrEmpty("Properties");
        return new Properties(p.getBooleanOr("cold", false), p.getFloatOr("mossiness", 0.0F), p.getBooleanOr("air_pocket", false),
            p.getBooleanOr("overgrown", false), p.getBooleanOr("vines", false), p.getBooleanOr("replace_with_blackstone", false));
    }
    private static ProcessorRule swap(Block source, float odds, Block target) {
        return new ProcessorRule(new RandomBlockMatchTest(source, odds), AlwaysTrueTest.INSTANCE, target.defaultBlockState());
    }
    private static ProcessorRule swap(Block source, Block target) {
        return new ProcessorRule(new BlockMatchTest(source), AlwaysTrueTest.INSTANCE, target.defaultBlockState());
    }
    private static int surfaceY(LevelAccessor level, int x, int z, VerticalPlacement placement) {
        return level.getHeight(getHeightMapType(placement), x, z) - 1;
    }
    private boolean replaceable(BlockState state) {
        return !state.is(Blocks.AIR) && !state.is(Blocks.OBSIDIAN) && !state.is(BlockTags.FEATURES_CANNOT_REPLACE)
            && (verticalPlacement == VerticalPlacement.IN_NETHER || !state.is(Blocks.LAVA));
    }
    private void laySoil(RandomSource random, LevelAccessor level, BlockPos pos) {
        if (!replaceable(level.getBlockState(pos))) {
            return;
        }
        boolean magma = !properties.cold() && random.nextFloat() < 0.07F;
        level.setBlockAndUpdate(pos, (magma ? Blocks.MAGMA_BLOCK : Blocks.NETHERRACK).defaultBlockState());
    }
    private void dripColumn(RandomSource random, LevelAccessor level, BlockPos start) {
        BlockPos.MutableBlockPos cursor = start.mutable();
        laySoil(random, level, cursor);
        for (int left = 8; left > 0 && random.nextFloat() < 0.5F; left--) {
            cursor.move(Direction.DOWN);
            laySoil(random, level, cursor);
        }
    }
    private void leavesAbove(RandomSource random, LevelAccessor level, BlockPos pos) {
        boolean lucky = random.nextFloat() < 0.5F;
        if (lucky && level.getBlockState(pos).is(Blocks.NETHERRACK) && level.getBlockState(pos.above()).isAir()) {
            level.setBlockAndUpdate(pos.above(), StateProps.with(Blocks.JUNGLE_LEAVES.defaultBlockState(), "persistent", true));
        }
    }
    private void vineOn(RandomSource random, LevelAccessor level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        if (state.isAir() || state.is(Blocks.VINE)) {
            return;
        }
        Direction side = getRandomHorizontalDirection(random);
        BlockPos next = pos.relative(side);
        if (level.getBlockState(next).isAir() && Block.isFaceFull(state.getCollisionShape(level, pos), side)) {
            level.setBlockAndUpdate(next, StateProps.with(Blocks.VINE.defaultBlockState(), side.getOpposite().getSerializedName(), true));
        }
    }
    private void spread(RandomSource random, LevelAccessor level) {
        boolean followGround = verticalPlacement == VerticalPlacement.ON_LAND_SURFACE || verticalPlacement == VerticalPlacement.ON_OCEAN_FLOOR;
        BlockPos center = boundingBox.getCenter();
        int range = kSpread.length;
        int meanWidth = (boundingBox.getXSpan() + boundingBox.getZSpan()) / 2;
        int shrink = random.nextInt(Math.max(1, 8 - meanWidth / 2));
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int x = center.getX() - range; x <= center.getX() + range; x++) {
            for (int z = center.getZ() - range; z <= center.getZ() + range; z++) {
                int dist = Math.max(0, Math.abs(x - center.getX()) + Math.abs(z - center.getZ()) + shrink);
                if (dist >= range || random.nextDouble() >= (double) kSpread[dist]) {
                    continue;
                }
                int ground = surfaceY(level, x, z, verticalPlacement);
                int y = followGround ? ground : Math.min(boundingBox.minY(), ground);
                pos.set(x, y, z);
                if (Math.abs(y - boundingBox.minY()) <= 3 && replaceable(level.getBlockState(pos))) {
                    laySoil(random, level, pos);
                    if (properties.overgrown()) {
                        leavesAbove(random, level, pos);
                    }
                    dripColumn(random, level, pos.below());
                }
            }
        }
    }
    private void dripBelow(RandomSource random, LevelAccessor level) {
        for (int x = boundingBox.minX() + 1; x < boundingBox.maxX(); x++) {
            for (int z = boundingBox.minZ() + 1; z < boundingBox.maxZ(); z++) {
                BlockPos pos = new BlockPos(x, boundingBox.minY(), z);
                BlockState here = level.getBlockState(pos);
                if (here.is(Blocks.NETHERRACK)) {
                    dripColumn(random, level, pos.below());
                }
            }
        }
    }
    public RuinedPortalPiece(HolderLookup.Provider registries, StructureTemplateManager structureTemplateManager, BlockPos templatePosition, VerticalPlacement verticalPlacement, Properties properties, Identifier templateLocation, StructureTemplate template, Rotation rotation, Mirror mirror, BlockPos pivot) {
        super(StructurePieceType.RUINED_PORTAL, 0, structureTemplateManager, templateLocation, templateLocation.toString(), settingsFor(registries, mirror, rotation, verticalPlacement, pivot, properties), templatePosition);
        this.verticalPlacement = verticalPlacement;
        this.properties = properties;
    }
    public RuinedPortalPiece(StructurePieceSerializationContext context, CompoundTag tag) {
        super(StructurePieceType.RUINED_PORTAL, tag, context.structureTemplateManager(), location -> settingsFromTag(context, tag, location));
        this.verticalPlacement = readPlacement(tag);
        this.properties = readProperties(tag);
    }
    protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
        super.addAdditionalSaveData(context, tag);
        tag.putString("Rotation", placeSettings.getRotation().name());
        tag.putString("Mirror", placeSettings.getMirror().name());
        tag.putString("VerticalPlacement", verticalPlacement.getSerializedName());
        CompoundTag p = new CompoundTag();
        p.putBoolean("cold", properties.cold());
        p.putFloat("mossiness", properties.mossiness());
        p.putBoolean("air_pocket", properties.airPocket());
        p.putBoolean("overgrown", properties.overgrown());
        p.putBoolean("vines", properties.vines());
        p.putBoolean("replace_with_blackstone", properties.replaceWithBlackstone());
        tag.put("Properties", p);
    }
    public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
        BoundingBox box = template.getBoundingBox(placeSettings, templatePosition);
        if (!chunkBB.isInside(box.getCenter())) {
            return;
        }
        chunkBB.encapsulate(box);
        super.postProcess(level, structureManager, generator, random, chunkBB, chunkPos, referencePos);
        spread(random, level);
        dripBelow(random, level);
        if (properties.vines() || properties.overgrown()) {
            BlockPos.betweenClosedStream(getBoundingBox()).forEach(pos -> {
                if (properties.vines()) {
                    vineOn(random, level, pos);
                }
                if (properties.overgrown()) {
                    leavesAbove(random, level, pos);
                }
            });
        }
    }
    protected void handleDataMarker(String markerId, BlockPos pos, ServerLevelAccessor level, RandomSource random, BoundingBox chunkBB) {
        return;
    }
    public static Heightmap.Types getHeightMapType(VerticalPlacement verticalPlacement) {
        return verticalPlacement == VerticalPlacement.ON_OCEAN_FLOOR ? Heightmap.Types.OCEAN_FLOOR_WG : Heightmap.Types.WORLD_SURFACE_WG;
    }
    public static record Properties(boolean cold, float mossiness, boolean airPocket, boolean overgrown, boolean vines, boolean replaceWithBlackstone) {
        public static final Codec<Properties> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.structures.RuinedPortalPiece.Properties.class, "CODEC");
    }
    public static enum VerticalPlacement implements StringRepresentable {
        ON_LAND_SURFACE, PARTLY_BURIED, ON_OCEAN_FLOOR, IN_MOUNTAIN, UNDERGROUND, IN_NETHER;
        public static final Codec<VerticalPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.structures.RuinedPortalPiece.VerticalPlacement.class, "CODEC");
        public String getName() {
            return getSerializedName();


        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}

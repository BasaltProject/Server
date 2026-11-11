// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.templatesystem.*;
import net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity.AppendLoot;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.RandomizableContainer;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.Mob;
import net.minecraft.tags.FluidTags;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.resources.ResourceKey;
import org.basaltmc.bukkit.facade.StateProps;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.util.Mth;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.TemplateStructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public class OceanRuinPieces {
    private static final Identifier[] kWarmRuins = ruins("warm_1", "warm_2", "warm_3", "warm_4", "warm_5", "warm_6", "warm_7", "warm_8");
    private static final Identifier[] kRuinsBrick = ruins("brick_1", "brick_2", "brick_3", "brick_4", "brick_5", "brick_6", "brick_7", "brick_8");
    private static final Identifier[] kRuinsCracked = ruins("cracked_1", "cracked_2", "cracked_3", "cracked_4", "cracked_5", "cracked_6", "cracked_7", "cracked_8");
    private static final Identifier[] kRuinsMossy = ruins("mossy_1", "mossy_2", "mossy_3", "mossy_4", "mossy_5", "mossy_6", "mossy_7", "mossy_8");
    private static final Identifier[] kBigRuinsBrick = ruins("big_brick_1", "big_brick_2", "big_brick_3", "big_brick_8");
    private static final Identifier[] kBigRuinsMossy = ruins("big_mossy_1", "big_mossy_2", "big_mossy_3", "big_mossy_8");
    private static final Identifier[] kBigRuinsCracked = ruins("big_cracked_1", "big_cracked_2", "big_cracked_3", "big_cracked_8");
    private static final Identifier[] kBigWarmRuins = ruins("big_warm_4", "big_warm_5", "big_warm_6", "big_warm_7");
    private static Identifier[] ruins(String... names) {
        Identifier[] out = new Identifier[names.length];
        for (int i = 0; i < names.length; i++) {
            out[i] = Identifier.withDefaultNamespace("underwater_ruin/" + names[i]);
        }
        return out;
    }
    private static StructureProcessor archaeology(Block candidate, Block replacement, ResourceKey<LootTable> table) {
        ProcessorRule rule = new ProcessorRule(new BlockMatchTest(candidate), AlwaysTrueTest.INSTANCE, PosAlwaysTrueTest.INSTANCE, replacement.defaultBlockState(), new AppendLoot(table));
        return new CappedProcessor(new RuleProcessor(List.of(rule)), ConstantInt.of(5));
    }
    private static final StructureProcessor kWarmDig = archaeology(Blocks.SAND, Blocks.SUSPICIOUS_SAND, BuiltInLootTables.OCEAN_RUIN_WARM_ARCHAEOLOGY);
    private static final StructureProcessor kColdDig = archaeology(Blocks.GRAVEL, Blocks.SUSPICIOUS_GRAVEL, BuiltInLootTables.OCEAN_RUIN_COLD_ARCHAEOLOGY);
    private static Identifier pick(Identifier[] pool, RandomSource random) {
        return pool[random.nextInt(pool.length)];
    }
    private static void cluster(StructureTemplateManager manager, RandomSource random, Rotation rotation, BlockPos p, OceanRuinStructure structure, StructurePiecesBuilder builder) {
        BlockPos mainPos = new BlockPos(p.getX(), 90, p.getZ());
        BlockPos mainFar = StructureTemplate.transform(new BlockPos(15, 0, 15), Mirror.NONE, rotation, BlockPos.ZERO).offset(mainPos);
        BoundingBox mainBox = BoundingBox.fromCorners(mainPos, mainFar);
        BlockPos anchor = new BlockPos(Math.min(mainPos.getX(), mainFar.getX()), mainPos.getY(), Math.min(mainPos.getZ(), mainFar.getZ()));
        List<BlockPos> spots = spots(random, anchor);
        int count = Mth.nextInt(random, 4, 8);
        for (int i = 0; i < count && !spots.isEmpty(); i++) {
            BlockPos pos = spots.remove(random.nextInt(spots.size()));
            Rotation turn = Rotation.getRandom(random);
            BlockPos far = StructureTemplate.transform(new BlockPos(5, 0, 6), Mirror.NONE, turn, BlockPos.ZERO).offset(pos);
            if (!BoundingBox.fromCorners(pos, far).intersects(mainBox)) {
                place(manager, pos, turn, builder, random, structure, false, 0.8F);
            }
        }
    }
    private static List<BlockPos> spots(RandomSource random, BlockPos o) {
        List<BlockPos> out = new ArrayList<>();
        out.add(o.offset(-16 + Mth.nextInt(random, 1, 8), 0, 16 + Mth.nextInt(random, 1, 7)));
        out.add(o.offset(-16 + Mth.nextInt(random, 1, 8), 0, Mth.nextInt(random, 1, 7)));
        out.add(o.offset(-16 + Mth.nextInt(random, 1, 8), 0, -16 + Mth.nextInt(random, 4, 8)));
        out.add(o.offset(Mth.nextInt(random, 1, 7), 0, 16 + Mth.nextInt(random, 1, 7)));
        out.add(o.offset(Mth.nextInt(random, 1, 7), 0, -16 + Mth.nextInt(random, 4, 6)));
        out.add(o.offset(16 + Mth.nextInt(random, 1, 7), 0, 16 + Mth.nextInt(random, 3, 8)));
        out.add(o.offset(16 + Mth.nextInt(random, 1, 7), 0, Mth.nextInt(random, 1, 7)));
        out.add(o.offset(16 + Mth.nextInt(random, 1, 7), 0, -16 + Mth.nextInt(random, 4, 8)));
        return out;
    }
    private static void place(StructureTemplateManager manager, BlockPos position, Rotation rotation, StructurePiecesBuilder builder, RandomSource random, OceanRuinStructure structure, boolean large, float base) {
        OceanRuinStructure.Type kind = structure.biomeTemp;
        if (kind == OceanRuinStructure.Type.COLD) {
            Identifier[] bricks = large ? kBigRuinsBrick : kRuinsBrick;
            Identifier[] cracked = large ? kBigRuinsCracked : kRuinsCracked;
            Identifier[] mossy = large ? kBigRuinsMossy : kRuinsMossy;
            int idx = random.nextInt(bricks.length);
            builder.addPiece(new OceanRuinPiece(manager, bricks[idx], position, rotation, base, kind, large));
            builder.addPiece(new OceanRuinPiece(manager, cracked[idx], position, rotation, 0.7F, kind, large));
            builder.addPiece(new OceanRuinPiece(manager, mossy[idx], position, rotation, 0.5F, kind, large));
        } else {
            Identifier start = pick(large ? kBigWarmRuins : kWarmRuins, random);
            builder.addPiece(new OceanRuinPiece(manager, start, position, rotation, base, kind, large));
        }
    }
    public static void addPieces(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation, StructurePiecesBuilder builder, RandomSource random, OceanRuinStructure structure) {
        boolean large = random.nextFloat() <= structure.largeProbability;
        float base = large ? 0.9F : 0.8F;
        place(structureTemplateManager, position, rotation, builder, random, structure, large, base);
        if (large && random.nextFloat() <= structure.clusterProbability) {
            cluster(structureTemplateManager, random, rotation, position, structure, builder);
        }
    }
    public static class OceanRuinPiece extends TemplateStructurePiece {
        private OceanRuinStructure.Type biomeType;
        private float integrity;
        private boolean isLarge;
        private static StructurePlaceSettings settingsFor(Rotation rotation, float integrity, OceanRuinStructure.Type biomeType) {
            StructureProcessor dig = biomeType == OceanRuinStructure.Type.COLD ? kColdDig : kWarmDig;
            return new StructurePlaceSettings().setRotation(rotation).setMirror(Mirror.NONE).addProcessor(new BlockRotProcessor(integrity)).addProcessor(BlockIgnoreProcessor.STRUCTURE_AND_AIR).addProcessor(dig);
        }
        private int settledY(BlockPos pos, BlockGetter level, BlockPos far) {
            int newY = pos.getY();
            int lowest = 512;
            int top = newY - 1;
            int uneven = 0;
            for (BlockPos p : BlockPos.betweenClosed(pos, far)) {
                int floorY = pos.getY() - 1;
                BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos(p.getX(), floorY, p.getZ());
                while (floorY > level.getMinY() + 1) {
                    BlockState state = level.getBlockState(probe);
                    if (!(state.isAir() || level.getFluidState(probe).is(FluidTags.WATER) || state.is(BlockTags.ICE))) {
                        break;
                    }
                    floorY--;
                    probe.set(p.getX(), floorY, p.getZ());
                }
                lowest = Math.min(lowest, floorY);
                if (floorY < top - 2) {
                    uneven++;
                }
            }
            int span = Math.abs(pos.getX() - far.getX());
            return top - lowest > 2 && uneven > span - 2 ? lowest + 1 : newY;
        }

        public OceanRuinPiece(StructureTemplateManager structureTemplateManager, Identifier templateLocation, BlockPos position, Rotation rotation, float integrity, OceanRuinStructure.Type biomeType, boolean isLarge) {
            super(StructurePieceType.OCEAN_RUIN, 0, structureTemplateManager, templateLocation, templateLocation.toString(), settingsFor(rotation, integrity, biomeType), position);
            this.integrity = integrity;
            this.biomeType = biomeType;
            this.isLarge = isLarge;
        }
        private OceanRuinPiece(StructureTemplateManager structureTemplateManager, CompoundTag tag, Rotation rotation, float integrity, OceanRuinStructure.Type biomeType, boolean isLarge) {
            super(StructurePieceType.OCEAN_RUIN, tag, structureTemplateManager, location -> settingsFor(rotation, integrity, biomeType));
            this.integrity = integrity;
            this.biomeType = biomeType;
            this.isLarge = isLarge;
        }
        public static OceanRuinPiece create(StructureTemplateManager structureTemplateManager, CompoundTag tag) {
            Rotation rotation = Rotation.valueOf(tag.getStringOr("Rot", "NONE"));
            float integrity = tag.getFloatOr("Integrity", 0.0F);
            OceanRuinStructure.Type biome = OceanRuinStructure.Type.valueOf(tag.getStringOr("BiomeType", "warm").toUpperCase(java.util.Locale.ROOT));
            return new OceanRuinPiece(structureTemplateManager, tag, rotation, integrity, biome, tag.getBooleanOr("IsLarge", false));
        }
        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putString("Rot", placeSettings.getRotation().name());
            tag.putFloat("Integrity", integrity);
            tag.putString("BiomeType", biomeType.getSerializedName());
            tag.putBoolean("IsLarge", isLarge);
        }
        protected void handleDataMarker(String markerId, BlockPos position, ServerLevelAccessor level, RandomSource random, BoundingBox chunkBB) {
            if ("chest".equals(markerId)) {
                boolean wet = level.getFluidState(position).is(FluidTags.WATER);
                level.setBlock(position, StateProps.with(Blocks.CHEST.defaultBlockState(), "waterlogged", wet), 2);
                if (level.getBlockEntity(position) instanceof RandomizableContainer chest) {
                    chest.setLootTable(isLarge ? BuiltInLootTables.UNDERWATER_RUIN_BIG : BuiltInLootTables.UNDERWATER_RUIN_SMALL, random.nextLong());
                }
            } else if ("drowned".equals(markerId) && EntityTypes.DROWNED.create(level.getLevel(), EntitySpawnReason.STRUCTURE) instanceof Mob drowned) {
                drowned.setPersistenceRequired();
                drowned.snapTo(position, 0.0F, 0.0F);
                drowned.finalizeSpawn(level, level.getCurrentDifficultyAt(position), EntitySpawnReason.STRUCTURE, null);
                level.addFreshEntityWithPassengers(drowned);
                level.setBlock(position, (position.getY() > level.getSeaLevel() ? Blocks.AIR : Blocks.WATER).defaultBlockState(), 2);
            }
        }
        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            int floor = level.getHeight(Heightmap.Types.OCEAN_FLOOR_WG, templatePosition.getX(), templatePosition.getZ());
            templatePosition = new BlockPos(templatePosition.getX(), floor, templatePosition.getZ());
            Vec3i size = template.getSize();
            BlockPos far = StructureTemplate.transform(new BlockPos(size.getX() - 1, 0, size.getZ() - 1), Mirror.NONE, placeSettings.getRotation(), BlockPos.ZERO).offset(templatePosition);
            templatePosition = new BlockPos(templatePosition.getX(), settledY(templatePosition, level, far), templatePosition.getZ());
            super.postProcess(level, structureManager, generator, random, chunkBB, chunkPos, referencePos);
        }
    }
}

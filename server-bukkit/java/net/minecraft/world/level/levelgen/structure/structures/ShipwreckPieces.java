// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockIgnoreProcessor;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import net.minecraft.world.RandomizableContainer;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.core.Vec3i;
import java.util.Map;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.block.Mirror;
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
public class ShipwreckPieces {
    private static final BlockPos kPivot = new BlockPos(4, 0, 15);
    private static final Identifier[] kBeached = {Identifier.withDefaultNamespace("shipwreck/with_mast"), Identifier.withDefaultNamespace("shipwreck/sideways_full"), Identifier.withDefaultNamespace("shipwreck/sideways_fronthalf"), Identifier.withDefaultNamespace("shipwreck/sideways_backhalf"), Identifier.withDefaultNamespace("shipwreck/rightsideup_full"), Identifier.withDefaultNamespace("shipwreck/rightsideup_fronthalf"), Identifier.withDefaultNamespace("shipwreck/rightsideup_backhalf"), Identifier.withDefaultNamespace("shipwreck/with_mast_degraded"), Identifier.withDefaultNamespace("shipwreck/rightsideup_full_degraded"), Identifier.withDefaultNamespace("shipwreck/rightsideup_fronthalf_degraded"), Identifier.withDefaultNamespace("shipwreck/rightsideup_backhalf_degraded")};
    private static final Identifier[] kOcean = {Identifier.withDefaultNamespace("shipwreck/with_mast"), Identifier.withDefaultNamespace("shipwreck/upsidedown_full"), Identifier.withDefaultNamespace("shipwreck/upsidedown_fronthalf"), Identifier.withDefaultNamespace("shipwreck/upsidedown_backhalf"), Identifier.withDefaultNamespace("shipwreck/sideways_full"), Identifier.withDefaultNamespace("shipwreck/sideways_fronthalf"), Identifier.withDefaultNamespace("shipwreck/sideways_backhalf"), Identifier.withDefaultNamespace("shipwreck/rightsideup_full"), Identifier.withDefaultNamespace("shipwreck/rightsideup_fronthalf"), Identifier.withDefaultNamespace("shipwreck/rightsideup_backhalf"), Identifier.withDefaultNamespace("shipwreck/with_mast_degraded"), Identifier.withDefaultNamespace("shipwreck/upsidedown_full_degraded"), Identifier.withDefaultNamespace("shipwreck/upsidedown_fronthalf_degraded"), Identifier.withDefaultNamespace("shipwreck/upsidedown_backhalf_degraded"), Identifier.withDefaultNamespace("shipwreck/sideways_full_degraded"), Identifier.withDefaultNamespace("shipwreck/sideways_fronthalf_degraded"), Identifier.withDefaultNamespace("shipwreck/sideways_backhalf_degraded"), Identifier.withDefaultNamespace("shipwreck/rightsideup_full_degraded"), Identifier.withDefaultNamespace("shipwreck/rightsideup_fronthalf_degraded"), Identifier.withDefaultNamespace("shipwreck/rightsideup_backhalf_degraded")};
    private static final Map<String, ResourceKey<LootTable>> kMarkers = Map.of("map_chest", BuiltInLootTables.SHIPWRECK_MAP,
        "treasure_chest", BuiltInLootTables.SHIPWRECK_TREASURE, "supply_chest", BuiltInLootTables.SHIPWRECK_SUPPLY);
    public static ShipwreckPiece addRandomPiece(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation, StructurePiecesBuilder builder, RandomSource random, boolean isBeached) {
        Identifier[] pool = isBeached ? kBeached : kOcean;
        ShipwreckPiece piece = new ShipwreckPiece(structureTemplateManager, pool[random.nextInt(pool.length)], position, rotation, isBeached);
        builder.addPiece(piece);
        return piece;
    }
    public static class ShipwreckPiece extends TemplateStructurePiece {
        private boolean isBeached;
        private boolean heightAdjusted;
        private static StructurePlaceSettings settingsFor(Rotation rotation) {
            return new StructurePlaceSettings().setRotation(rotation).setMirror(Mirror.NONE).setRotationPivot(kPivot).addProcessor(BlockIgnoreProcessor.STRUCTURE_AND_AIR);
        }

        public ShipwreckPiece(StructureTemplateManager structureTemplateManager, Identifier templateLocation, BlockPos position, Rotation rotation, boolean isBeached) {
            super(StructurePieceType.SHIPWRECK_PIECE, 0, structureTemplateManager, templateLocation, templateLocation.toString(), settingsFor(rotation), position);
            this.isBeached = isBeached;
        }
        public ShipwreckPiece(StructureTemplateManager structureTemplateManager, CompoundTag tag) {
            super(StructurePieceType.SHIPWRECK_PIECE, tag, structureTemplateManager, location -> settingsFor(Rotation.valueOf(tag.getStringOr("Rot", "NONE"))));
            this.isBeached = tag.getBooleanOr("isBeached", false);
            this.heightAdjusted = tag.getBooleanOr("height_adjusted", false);
        }
        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("isBeached", this.isBeached);
            tag.putString("Rot", this.placeSettings.getRotation().name());
            tag.putBoolean("height_adjusted", this.heightAdjusted);
        }
        protected void handleDataMarker(String markerId, BlockPos position, ServerLevelAccessor level, RandomSource random, BoundingBox chunkBB) {
            ResourceKey<LootTable> table = kMarkers.get(markerId);
            if (table != null && level.getBlockEntity(position.below()) instanceof RandomizableContainer container) {
                container.setLootTable(table, random.nextLong());
            }
        }
        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            if (!heightAdjusted && !isTooBigToFitInWorldGenRegion()) {
                Heightmap.Types map = isBeached ? Heightmap.Types.WORLD_SURFACE_WG : Heightmap.Types.OCEAN_FLOOR_WG;
                Vec3i size = template.getSize();
                int area = size.getX() * size.getZ();
                int lowest = level.getMaxY() + 1;
                int mean = 0;
                if (area == 0) {
                    mean = level.getHeight(map, templatePosition.getX(), templatePosition.getZ());
                } else {
                    BlockPos far = templatePosition.offset(size.getX() - 1, 0, size.getZ() - 1);
                    for (BlockPos p : BlockPos.betweenClosed(templatePosition, far)) {
                        int h = level.getHeight(map, p.getX(), p.getZ());
                        mean += h;
                        lowest = Math.min(lowest, h);
                    }
                    mean /= area;
                }
                adjustPositionHeight(isBeached ? calculateBeachedPosition(lowest, random) : mean);
            }
            super.postProcess(level, structureManager, generator, random, chunkBB, chunkPos, referencePos);
        }
        public boolean isTooBigToFitInWorldGenRegion() {
            Vec3i size = template.getSize();
            return size.getX() > 32 || size.getY() > 32;
        }
        public int calculateBeachedPosition(int minY, RandomSource random) {
            return minY - template.getSize().getY() / 2 - random.nextInt(3);
        }
        public void adjustPositionHeight(int newHeight) {
            heightAdjusted = true;
            templatePosition = new BlockPos(templatePosition.getX(), newHeight, templatePosition.getZ());


        }
    }
}

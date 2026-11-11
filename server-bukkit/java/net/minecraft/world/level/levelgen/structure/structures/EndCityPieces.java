// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.decoration.ItemFrame;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.Level;
import net.minecraft.world.RandomizableContainer;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.TemplateStructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockIgnoreProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;

public class EndCityPieces {
    private static final int kMaxDepth = 8;
    private static final Rotation[] kTurns = {Rotation.NONE, Rotation.CLOCKWISE_90, Rotation.COUNTERCLOCKWISE_90, Rotation.CLOCKWISE_180};
    private static final BlockPos[] kTowerLinks = {new BlockPos(1, -1, 0), new BlockPos(6, -1, 1), new BlockPos(0, -1, 5), new BlockPos(5, -1, 6)};
    private static final BlockPos[] kFatLinks = {new BlockPos(4, -1, 0), new BlockPos(12, -1, 4), new BlockPos(0, -1, 8), new BlockPos(8, -1, 12)};

    interface SectionGenerator {
        void init();

        boolean generate(StructureTemplateManager structureTemplateManager, int genDepth, EndCityPiece parent, BlockPos offset, List<StructurePiece> pieces, RandomSource random);
    }

    private static final SectionGenerator kHouse = new SectionGenerator() {
        public void init() {
            return;
        }

        public boolean generate(StructureTemplateManager manager, int depth, EndCityPiece parent, BlockPos offset, List<StructurePiece> pieces, RandomSource random) {
            if (depth > kMaxDepth) {
                return false;
            }
            Rotation turn = parent.placeSettings().getRotation();
            EndCityPiece last = push(pieces, attach(manager, parent, offset, "base_floor", turn, true));
            int floors = random.nextInt(3);
            if (floors == 0) {
                push(pieces, attach(manager, last, new BlockPos(-1, 4, -1), "base_roof", turn, true));
            } else if (floors == 1) {
                last = push(pieces, attach(manager, last, new BlockPos(-1, 0, -1), "second_floor_2", turn, false));
                last = push(pieces, attach(manager, last, new BlockPos(-1, 8, -1), "second_roof", turn, false));
                descend(manager, kTower, depth + 1, last, null, pieces, random);
            } else {
                last = push(pieces, attach(manager, last, new BlockPos(-1, 0, -1), "second_floor_2", turn, false));
                last = push(pieces, attach(manager, last, new BlockPos(-1, 4, -1), "third_floor_2", turn, false));
                last = push(pieces, attach(manager, last, new BlockPos(-1, 8, -1), "third_roof", turn, true));
                descend(manager, kTower, depth + 1, last, null, pieces, random);
            }
            return true;
        }
    };

    private static final SectionGenerator kTower = new SectionGenerator() {
        public void init() {
            return;
        }

        public boolean generate(StructureTemplateManager manager, int depth, EndCityPiece parent, BlockPos offset, List<StructurePiece> pieces, RandomSource random) {
            Rotation turn = parent.placeSettings().getRotation();
            EndCityPiece last = push(pieces, attach(manager, parent, new BlockPos(3 + random.nextInt(2), -3, 3 + random.nextInt(2)), "tower_base", turn, true));
            last = push(pieces, attach(manager, last, new BlockPos(0, 7, 0), "tower_piece", turn, true));
            EndCityPiece bridgeBase = random.nextInt(3) == 0 ? last : null;
            int height = 1 + random.nextInt(3);
            for (int i = 0; i < height; i++) {
                last = push(pieces, attach(manager, last, new BlockPos(0, 4, 0), "tower_piece", turn, true));
                if (i < height - 1 && random.nextBoolean()) {
                    bridgeBase = last;
                }
            }
            if (bridgeBase != null) {
                for (int i = 0; i < kTurns.length; i++) {
                    if (random.nextBoolean()) {
                        EndCityPiece start = push(pieces, attach(manager, bridgeBase, kTowerLinks[i], "bridge_end", turn.getRotated(kTurns[i]), true));
                        descend(manager, kBridge, depth + 1, start, null, pieces, random);
                    }
                }
                push(pieces, attach(manager, last, new BlockPos(-1, 4, -1), "tower_top", turn, true));
            } else if (depth != 7) {
                return descend(manager, kFat, depth + 1, last, null, pieces, random);
            } else {
                push(pieces, attach(manager, last, new BlockPos(-1, 4, -1), "tower_top", turn, true));
            }
            return true;
        }
    };

    private static final SectionGenerator kBridge = new SectionGenerator() {
        private boolean shipPlaced;

        public void init() {
            shipPlaced = false;
        }

        public boolean generate(StructureTemplateManager manager, int depth, EndCityPiece parent, BlockPos offset, List<StructurePiece> pieces, RandomSource random) {
            Rotation turn = parent.placeSettings().getRotation();
            int length = random.nextInt(4) + 1;
            EndCityPiece last = push(pieces, attach(manager, parent, new BlockPos(0, 0, -4), "bridge_piece", turn, true));
            last.setGenDepth(-1);
            int rise = 0;
            for (int i = 0; i < length; i++) {
                if (random.nextBoolean()) {
                    last = push(pieces, attach(manager, last, new BlockPos(0, rise, -4), "bridge_piece", turn, true));
                    rise = 0;
                    continue;
                }
                boolean steep = random.nextBoolean();
                last = push(pieces, attach(manager, last, new BlockPos(0, rise, steep ? -4 : -8), steep ? "bridge_steep_stairs" : "bridge_gentle_stairs", turn, true));
                rise = 4;
            }
            if (!shipPlaced && random.nextInt(10 - depth) == 0) {
                push(pieces, attach(manager, last, new BlockPos(-8 + random.nextInt(8), rise, -70 + random.nextInt(10)), "ship", turn, true));
                shipPlaced = true;
            } else if (!descend(manager, kHouse, depth + 1, last, new BlockPos(-3, rise + 1, -11), pieces, random)) {
                return false;
            }
            last = push(pieces, attach(manager, last, new BlockPos(4, rise, 0), "bridge_end", turn.getRotated(Rotation.CLOCKWISE_180), true));
            last.setGenDepth(-1);
            return true;
        }
    };

    private static final SectionGenerator kFat = new SectionGenerator() {
        public void init() {
            return;
        }

        public boolean generate(StructureTemplateManager manager, int depth, EndCityPiece parent, BlockPos offset, List<StructurePiece> pieces, RandomSource random) {
            Rotation turn = parent.placeSettings().getRotation();
            EndCityPiece last = push(pieces, attach(manager, parent, new BlockPos(-3, 4, -3), "fat_tower_base", turn, true));
            last = push(pieces, attach(manager, last, new BlockPos(0, 4, 0), "fat_tower_middle", turn, true));
            for (int i = 0; i < 2 && random.nextInt(3) != 0; i++) {
                last = push(pieces, attach(manager, last, new BlockPos(0, 8, 0), "fat_tower_middle", turn, true));
                for (int k = 0; k < kTurns.length; k++) {
                    if (random.nextBoolean()) {
                        EndCityPiece start = push(pieces, attach(manager, last, kFatLinks[k], "bridge_end", turn.getRotated(kTurns[k]), true));
                        descend(manager, kBridge, depth + 1, start, null, pieces, random);
                    }
                }
            }
            push(pieces, attach(manager, last, new BlockPos(-2, 8, -2), "fat_tower_top", turn, true));
            return true;
        }
    };

    private static EndCityPiece attach(StructureTemplateManager manager, EndCityPiece parent, BlockPos offset, String name, Rotation rotation, boolean overwrite) {
        EndCityPiece child = new EndCityPiece(manager, name, parent.templatePosition(), rotation, overwrite);
        BlockPos shift = parent.template().calculateConnectedPosition(parent.placeSettings(), offset, child.placeSettings(), BlockPos.ZERO);
        child.move(shift.getX(), shift.getY(), shift.getZ());
        return child;
    }

    private static EndCityPiece push(List<StructurePiece> pieces, EndCityPiece piece) {
        pieces.add(piece);
        return piece;
    }

    private static boolean descend(StructureTemplateManager manager, SectionGenerator generator, int depth, EndCityPiece parent, BlockPos offset, List<StructurePiece> pieces, RandomSource random) {
        if (depth > kMaxDepth) {
            return false;
        }
        List<StructurePiece> fresh = new ArrayList<>();
        if (!generator.generate(manager, depth, parent, offset, fresh, random)) {
            return false;
        }
        int tag = random.nextInt();
        for (StructurePiece child : fresh) {
            child.setGenDepth(tag);
            StructurePiece hit = StructurePiece.findCollisionPiece(pieces, child.getBoundingBox());
            if (hit != null && hit.getGenDepth() != parent.getGenDepth()) {
                return false;
            }
        }
        pieces.addAll(fresh);
        return true;
    }

    public static void startHouseTower(StructureTemplateManager structureTemplateManager, BlockPos origin, Rotation rotation, List<StructurePiece> pieces, RandomSource random) {
        kFat.init();
        kHouse.init();
        kBridge.init();
        kTower.init();
        EndCityPiece last = push(pieces, new EndCityPiece(structureTemplateManager, "base_floor", origin, rotation, true));
        last = push(pieces, attach(structureTemplateManager, last, new BlockPos(-1, 0, -1), "second_floor_1", rotation, false));
        last = push(pieces, attach(structureTemplateManager, last, new BlockPos(-1, 4, -1), "third_floor_1", rotation, false));
        last = push(pieces, attach(structureTemplateManager, last, new BlockPos(-1, 8, -1), "third_roof", rotation, true));
        descend(structureTemplateManager, kTower, 1, last, null, pieces, random);
    }

    public static class EndCityPiece extends TemplateStructurePiece {
        public EndCityPiece(StructureTemplateManager structureTemplateManager, String templateName, BlockPos position, Rotation rotation, boolean overwrite) {
            super(StructurePieceType.END_CITY_PIECE, 0, structureTemplateManager, idFor(templateName), templateName, settingsFor(overwrite, rotation), position);
        }

        public EndCityPiece(StructureTemplateManager structureTemplateManager, CompoundTag tag) {
            super(StructurePieceType.END_CITY_PIECE, tag, structureTemplateManager,
                location -> settingsFor(tag.getBooleanOr("OW", false), Rotation.valueOf(tag.getStringOr("Rot", "NONE"))));
        }

        private static StructurePlaceSettings settingsFor(boolean overwrite, Rotation rotation) {
            BlockIgnoreProcessor ignore = overwrite ? BlockIgnoreProcessor.STRUCTURE_BLOCK : BlockIgnoreProcessor.STRUCTURE_AND_AIR;
            return new StructurePlaceSettings().setIgnoreEntities(true).addProcessor(ignore).setRotation(rotation);
        }

        private static Identifier idFor(String name) {
            return Identifier.withDefaultNamespace("end_city/" + name);
        }

        protected Identifier makeTemplateLocation() {
            return idFor(templateName);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putString("Rot", placeSettings.getRotation().name());
            tag.putBoolean("OW", placeSettings.getProcessors().get(0) == BlockIgnoreProcessor.STRUCTURE_BLOCK);
        }

        protected void handleDataMarker(String markerId, BlockPos position, ServerLevelAccessor level, RandomSource random, BoundingBox chunkBB) {
            if (markerId.startsWith("Chest")) {
                BlockPos chest = position.below();
                if (chunkBB.isInside(chest) && level.getBlockEntity(chest) instanceof RandomizableContainer container) {
                    container.setLootTable(BuiltInLootTables.END_CITY_TREASURE, random.nextLong());
                }
            } else if (chunkBB.isInside(position) && Level.isInSpawnableBounds(position)) {
                if (markerId.startsWith("Sentry")) {
                    var shulker = EntityTypes.SHULKER.create(level.getLevel(), EntitySpawnReason.STRUCTURE);
                    if (shulker != null) {
                        shulker.setPos((double) position.getX() + 0.5D, (double) position.getY(), (double) position.getZ() + 0.5D);
                        level.addFreshEntity(shulker);
                    }
                } else if (markerId.startsWith("Elytra")) {
                    ItemFrame frame = new ItemFrame(level.getLevel(), position, placeSettings.getRotation().rotate(Direction.SOUTH));
                    frame.setItem(new ItemStack(Items.ELYTRA), false);
                    level.addFreshEntity(frame);
                }
            }
        }
    }
}

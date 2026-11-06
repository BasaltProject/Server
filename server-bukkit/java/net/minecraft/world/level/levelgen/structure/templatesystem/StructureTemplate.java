// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import it.unimi.dsi.fastutil.objects.ObjectArrayList;
import java.util.Iterator;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.Vec3i;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.JigsawBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.DiscreteVoxelShape;
public class StructureTemplate {
    private CompoundTag data = new CompoundTag();
    private String author = "?";
    public static final String PALETTE_TAG = "palette";
    public static final String PALETTE_LIST_TAG = "palettes";
    public static final String ENTITIES_TAG = "entities";
    public static final String BLOCKS_TAG = "blocks";
    public static final String BLOCK_TAG_POS = "pos";
    public static final String BLOCK_TAG_STATE = "state";
    public static final String BLOCK_TAG_NBT = "nbt";
    public static final String ENTITY_TAG_POS = "pos";
    public static final String ENTITY_TAG_BLOCKPOS = "blockPos";
    public static final String ENTITY_TAG_NBT = "nbt";
    public static final String SIZE_TAG = "size";
    public StructureTemplate() {

    }
    public Vec3i getSize() {
        net.minecraft.nbt.ListTag size = data.getListOrEmpty("size");
        return size.size() < 3 ? Vec3i.ZERO : new Vec3i(size.getIntOr(0, 0), size.getIntOr(1, 0), size.getIntOr(2, 0));
    }
    public void setAuthor(String author) {
        this.author = author;
    }
    public String getAuthor() {
        return author;
    }
    public void fillFromWorld(Level level, BlockPos position, Vec3i size, boolean inludeEntities, List<Block> ignoreBlocks) {
        if (level instanceof net.minecraft.server.level.ServerLevel server) {
            org.basaltmc.bukkit.Blocks.flushAll();
            byte[] bytes = org.basaltmc.bukkit.LeftoverNatives.structureCapture(server.getWorld().getName(), position.getX(),
                position.getY(), position.getZ(), size.getX(), size.getY(), size.getZ(), inludeEntities);
            if (bytes != null) {
                data = org.basaltmc.bukkit.facade.NbtCodec.fromAnonymous(bytes);
            }
        }
    }
    private Object[] scan(BlockPos position, StructurePlaceSettings settings, String block, boolean absolute) {
        BlockPos pivot = settings.getRotationPivot();
        Object[] scan = org.basaltmc.bukkit.LeftoverNatives.templateScan(org.basaltmc.bukkit.facade.NbtCodec.toAnonymous(data),
            position.getX(), position.getY(), position.getZ(), settings.getRotation().ordinal(), settings.getMirror().ordinal(),
            pivot.getX(), pivot.getZ(), block, absolute);
        return scan == null ? new Object[] {new int[3], new int[0], new byte[0][], new int[0], new String[0]} : scan;
    }
    public List<StructureBlockInfo> filterBlocks(BlockPos position, StructurePlaceSettings settings, Block block) {
        return filterBlocks(position, settings, block, true);
    }
    public List<JigsawBlockInfo> getJigsaws(BlockPos position, Rotation rotation) {
        Object[] scan = scan(position, new StructurePlaceSettings().setRotation(rotation), "", true);
        int[] ints = (int[]) scan[3];
        String[] text = (String[]) scan[4];
        List<JigsawBlockInfo> found = new java.util.ArrayList<>();
        for (int at = 0; at * 6 < ints.length; at++) {
            String orientation = text[at * 6] + "_" + text[at * 6 + 1];
            BlockState state = net.minecraft.world.level.block.Blocks.JIGSAW.defaultBlockState().setValue(
                net.minecraft.world.level.block.state.properties.BlockStateProperties.ORIENTATION,
                net.minecraft.world.level.block.state.properties.BlockStateProperties.OrientationValue.valueOf(
                    orientation.toUpperCase(java.util.Locale.ROOT)));
            found.add(new JigsawBlockInfo(new BlockPos(ints[at * 6], ints[at * 6 + 1], ints[at * 6 + 2]), state,
                ints[at * 6 + 3] != 0 ? JigsawBlockEntity.JointType.ROLLABLE : JigsawBlockEntity.JointType.ALIGNED,
                Identifier.parse(text[at * 6 + 2]),
                (ResourceKey<StructureTemplatePool>) (ResourceKey) ResourceKey.create(
                    net.minecraft.core.registries.Registries.TEMPLATE_POOL, Identifier.parse(text[at * 6 + 3])),
                Identifier.parse(text[at * 6 + 4]), ints[at * 6 + 4], ints[at * 6 + 5]));
        }
        return found;
    }
    public ObjectArrayList<StructureBlockInfo> filterBlocks(BlockPos position, StructurePlaceSettings settings, Block block, boolean absolute) {
        Object[] scan = scan(position, settings, net.minecraft.core.registries.BuiltInRegistries.BLOCK.getKey(block).toString(), absolute);
        int[] ints = (int[]) scan[1];
        byte[][] tiles = (byte[][]) scan[2];
        BoundingBox box = settings.getBoundingBox();
        ObjectArrayList<StructureBlockInfo> result = new ObjectArrayList<>();
        for (int at = 0; at * 4 < ints.length; at++) {
            BlockPos pos = new BlockPos(ints[at * 4], ints[at * 4 + 1], ints[at * 4 + 2]);
            if (box == null || box.isInside(pos)) {
                result.add(new StructureBlockInfo(pos, Block.stateById(ints[at * 4 + 3]).rotate(settings.getRotation()),
                    tiles[at] == null ? null : org.basaltmc.bukkit.facade.NbtCodec.fromAnonymous(tiles[at])));
            }
        }
        return result;
    }
    public BlockPos calculateConnectedPosition(StructurePlaceSettings settings1, BlockPos connection1, StructurePlaceSettings settings2, BlockPos connection2) {
        return calculateRelativePosition(settings1, connection1).subtract(calculateRelativePosition(settings2, connection2));
    }
    public static BlockPos calculateRelativePosition(StructurePlaceSettings settings, BlockPos pos) {
        return transform(pos, settings.getMirror(), settings.getRotation(), settings.getRotationPivot());
    }
    public boolean placeInWorld(ServerLevelAccessor level, BlockPos position, BlockPos referencePos, StructurePlaceSettings settings, RandomSource random, int updateMode) {
        if (!(level.getLevel() instanceof net.minecraft.server.level.ServerLevel server)) {
            return false;
        }
        return org.basaltmc.bukkit.LeftoverNatives.structurePlace(org.basaltmc.bukkit.facade.NbtCodec.toAnonymous(data),
            server.getWorld().getName(), position.getX(), position.getY(), position.getZ(), settings.getRotation().ordinal(),
            settings.getMirror().ordinal(), 1.0F, settings.getRandom(position).nextLong(), !settings.isIgnoreEntities(), 0);
    }
    public static void updateShapeAtEdge(LevelAccessor level, int updateMode, DiscreteVoxelShape shape, BlockPos pos) {
        updateShapeAtEdge(level, updateMode, shape, pos.getX(), pos.getY(), pos.getZ());
    }
    public static void updateShapeAtEdge(LevelAccessor level, int updateMode, DiscreteVoxelShape shape, int startX, int startY, int startZ) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        BlockPos.MutableBlockPos neighbor = new BlockPos.MutableBlockPos();
        shape.forAllFaces((direction, x, y, z) -> {
            pos.set(startX + x, startY + y, startZ + z);
            neighbor.setWithOffset(pos, direction);
            BlockState state = level.getBlockState(pos);
            BlockState neighborState = level.getBlockState(neighbor);
            BlockState updated = state.updateShape(level, level, pos, direction, neighbor, neighborState, level.getRandom());
            if (state != updated) {
                level.setBlock(pos, updated, updateMode & -2);
            }
            BlockState neighborUpdated = neighborState.updateShape(level, level, neighbor, direction.getOpposite(), pos, updated,
                level.getRandom());
            if (neighborState != neighborUpdated) {
                level.setBlock(neighbor, neighborUpdated, updateMode & -2);
            }
        });
    }
    public static List<StructureBlockInfo> processBlockInfos(ServerLevelAccessor level, BlockPos position, BlockPos referencePos, StructurePlaceSettings settings, List<StructureBlockInfo> blockInfoList) {
        List<StructureBlockInfo> original = new java.util.ArrayList<>();
        List<StructureBlockInfo> processed = new java.util.ArrayList<>();
        boolean onlyInChunk = true;
        for (StructureProcessor processor : settings.getProcessors()) {
            if (processor.evaluatesEntirePieceState()) {
                onlyInChunk = false;
                break;
            }
        }
        BoundingBox chunkBox = settings.getBoundingBox();
        for (StructureBlockInfo info : blockInfoList) {
            BlockPos pos = calculateRelativePosition(settings, info.pos()).offset(position);
            if (onlyInChunk && chunkBox != null && !chunkBox.isInside(pos)) {
                continue;
            }
            StructureBlockInfo current = new StructureBlockInfo(pos, info.state(), info.nbt() == null ? null : info.nbt().copy());
            for (StructureProcessor processor : settings.getProcessors()) {
                if (current == null) {
                    break;
                }
                current = processor.processBlock(level, position, referencePos, info.pos(), current, settings);
            }
            if (current != null) {
                processed.add(current);
                original.add(info);
            }
        }
        for (StructureProcessor processor : settings.getProcessors()) {
            processed = processor.finalizeProcessing(level, position, referencePos, original, processed, settings);
        }
        return processed;
    }
    public Vec3i getSize(Rotation rotation) {
        Vec3i size = getSize();
        return rotation == Rotation.CLOCKWISE_90 || rotation == Rotation.COUNTERCLOCKWISE_90
            ? new Vec3i(size.getZ(), size.getY(), size.getX()) : size;
    }
    public static BlockPos transform(BlockPos pos, Mirror mirror, Rotation rotation, BlockPos pivot) {
        int dx = pos.getX() - pivot.getX();
        int dz = pos.getZ() - pivot.getZ();
        if (mirror == Mirror.LEFT_RIGHT) {
            dz = -dz;
        } else if (mirror == Mirror.FRONT_BACK) {
            dx = -dx;
        }
        int rx = dx;
        int rz = dz;
        switch (rotation) {
            case CLOCKWISE_90 -> {
                rx = -dz;
                rz = dx;
            }
            case CLOCKWISE_180 -> {
                rx = -dx;
                rz = -dz;
            }
            case COUNTERCLOCKWISE_90 -> {
                rx = dz;
                rz = -dx;
            }
            default -> {
            }
        }
        return new BlockPos(pivot.getX() + rx, pos.getY(), pivot.getZ() + rz);
    }
    public static Vec3 transform(Vec3 pos, Mirror mirror, Rotation rotation, BlockPos pivot) {
        double dx = pos.x - pivot.getX();
        double dz = pos.z - pivot.getZ();
        if (mirror == Mirror.LEFT_RIGHT) {
            dz = -dz;
        } else if (mirror == Mirror.FRONT_BACK) {
            dx = -dx;
        }
        double rx = dx;
        double rz = dz;
        switch (rotation) {
            case CLOCKWISE_90 -> {
                rx = -dz;
                rz = dx;
            }
            case CLOCKWISE_180 -> {
                rx = -dx;
                rz = -dz;
            }
            case COUNTERCLOCKWISE_90 -> {
                rx = dz;
                rz = -dx;
            }
            default -> {
            }
        }
        return new Vec3(pivot.getX() + rx, pos.y, pivot.getZ() + rz);
    }
    public BlockPos getZeroPositionWithTransform(BlockPos zeroPos, Mirror mirror, Rotation rotation) {
        return getZeroPositionWithTransform(zeroPos, mirror, rotation, getSize().getX(), getSize().getZ());
    }
    public static BlockPos getZeroPositionWithTransform(BlockPos zeroPos, Mirror mirror, Rotation rotation, int sizeX, int sizeZ) {
        int maxX = sizeX - 1;
        int maxZ = sizeZ - 1;
        int mirrorX = mirror == Mirror.FRONT_BACK ? maxX : 0;
        int mirrorZ = mirror == Mirror.LEFT_RIGHT ? maxZ : 0;
        return switch (rotation) {
            case CLOCKWISE_90 -> zeroPos.offset(mirrorZ, 0, maxX - mirrorX);
            case COUNTERCLOCKWISE_90 -> zeroPos.offset(maxZ - mirrorZ, 0, mirrorX);
            case CLOCKWISE_180 -> zeroPos.offset(maxX - mirrorX, 0, maxZ - mirrorZ);
            default -> zeroPos.offset(mirrorX, 0, mirrorZ);
        };
    }
    public BoundingBox getBoundingBox(StructurePlaceSettings settings, BlockPos position) {
        return getBoundingBox(position, settings.getRotation(), settings.getRotationPivot(), settings.getMirror());
    }
    public BoundingBox getBoundingBox(BlockPos position, Rotation rotation, BlockPos pivot, Mirror mirror) {
        return getBoundingBox(position, rotation, pivot, mirror, getSize());
    }
    protected static BoundingBox getBoundingBox(BlockPos position, Rotation rotation, BlockPos pivot, Mirror mirror, Vec3i size) {
        BlockPos far = position.offset(size.getX() - 1, size.getY() - 1, size.getZ() - 1);
        return BoundingBox.fromCorners(transform(position, mirror, rotation, pivot), transform(far, mirror, rotation, pivot));
    }
    public CompoundTag save(CompoundTag tag) {
        for (String key : data.keySet()) {
            tag.put(key, data.get(key));
        }
        return tag;
    }
    public void load(HolderGetter<Block> blockLookup, CompoundTag tag) {
        data = tag.copy();
    }
    public static JigsawBlockEntity.JointType getJointType(CompoundTag nbt, BlockState state) {
        String joint = nbt.getStringOr("joint", "");
        for (JigsawBlockEntity.JointType type : JigsawBlockEntity.JointType.values()) {
            if (type.getSerializedName().equals(joint)) {
                return type;
            }
        }
        return getDefaultJointType(state);
    }
    public static JigsawBlockEntity.JointType getDefaultJointType(BlockState state) {
        String front = state.getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.ORIENTATION).name();
        return front.startsWith("UP") || front.startsWith("DOWN") ? JigsawBlockEntity.JointType.ROLLABLE : JigsawBlockEntity.JointType.ALIGNED;
    }
    static class SimplePalette implements Iterable<BlockState> {
        public static final BlockState DEFAULT_BLOCK_STATE = net.minecraft.world.level.block.Blocks.AIR.defaultBlockState();
        private final java.util.List<BlockState> states = new java.util.ArrayList<>();
        private final java.util.Map<BlockState, Integer> ids = new java.util.HashMap<>();
        public int idFor(BlockState state) {
            Integer known = ids.get(state);
            if (known != null) {
                return known;
            }
            addMapping(state, states.size());
            return ids.get(state);
        }
        public BlockState stateFor(int index) {
            return index >= 0 && index < states.size() ? states.get(index) : DEFAULT_BLOCK_STATE;
        }
        public Iterator<BlockState> iterator() {
            return states.iterator();
        }
        public void addMapping(BlockState state, int id) {
            while (states.size() <= id) {
                states.add(null);
            }
            states.set(id, state);
            ids.put(state, id);
        }
    }
    public static record StructureBlockInfo(BlockPos pos, BlockState state, CompoundTag nbt) {
        public String toString() {
            return "StructureBlockInfo[pos=" + pos + ", state=" + state + ", nbt=" + nbt + "]";
        }
    }
    public static record JigsawBlockInfo(BlockPos pos, BlockState state, JigsawBlockEntity.JointType jointType, Identifier name, ResourceKey<StructureTemplatePool> pool, Identifier target, int placementPriority, int selectionPriority) {
        public static JigsawBlockInfo parse(StructureBlockInfo info) {
            CompoundTag nbt = java.util.Objects.requireNonNull(info.nbt(), () -> info + " nbt was null");
            Identifier none = Identifier.withDefaultNamespace("empty");
            return new JigsawBlockInfo(info.pos(), info.state(), StructureTemplate.getJointType(nbt, info.state()),
                Identifier.parse(nbt.getStringOr("name", none.toString())),
                (ResourceKey<StructureTemplatePool>) (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.TEMPLATE_POOL,
                    Identifier.parse(nbt.getStringOr("pool", none.toString()))),
                Identifier.parse(nbt.getStringOr("target", none.toString())), nbt.getIntOr("placement_priority", 0),
                nbt.getIntOr("selection_priority", 0));
        }
        public String toString() {
            return String.format(java.util.Locale.ROOT, "<JigsawBlockInfo | %s | %s | name: %s | pool: %s | target: %s | placement: %d | selection: %d>",
                pos, state, name, pool.identifier(), target, placementPriority, selectionPriority);
        }
        public JigsawBlockInfo withInfo(BlockPos pos, BlockState state) {
            return new JigsawBlockInfo(pos, state, jointType, name, pool, target, placementPriority, selectionPriority);
        }
    }
    public static class StructureEntityInfo {
        public final Vec3 pos;
        public final BlockPos blockPos;
        public final CompoundTag nbt;
        public StructureEntityInfo(Vec3 pos, BlockPos blockPos, CompoundTag nbt) {
            this.pos = pos;
            this.blockPos = blockPos;
            this.nbt = nbt;
        }
    }
    public static final class Palette {
        private final List<StructureBlockInfo> blocks;
        private Palette(List<StructureBlockInfo> blocks) {
            this.blocks = blocks;
        }
        public List<JigsawBlockInfo> jigsaws() {
            List<JigsawBlockInfo> found = new java.util.ArrayList<>();
            for (StructureBlockInfo info : blocks) {
                if (info.nbt() != null && info.state().is(net.minecraft.world.level.block.Blocks.JIGSAW)) {
                    found.add(JigsawBlockInfo.parse(info));
                }
            }
            return found;
        }
        public List<StructureBlockInfo> blocks() {
            return blocks;
        }
        public List<StructureBlockInfo> blocks(Block filter) {
            return blocks.stream().filter(info -> info.state().is(filter)).toList();
        }
    }
}

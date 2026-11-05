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
            byte[] bytes = org.basaltmc.bukkit.LeftoverNatives.structureCapture(server.getWorld().getName(), position.getX(),
                position.getY(), position.getZ(), size.getX(), size.getY(), size.getZ(), inludeEntities);
            if (bytes != null) {
                data = org.basaltmc.bukkit.facade.NbtCodec.fromBytes(bytes);
            }
        }
    }
    public List<StructureBlockInfo> filterBlocks(BlockPos position, StructurePlaceSettings settings, Block block) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate#filterBlocks");
    }
    public List<JigsawBlockInfo> getJigsaws(BlockPos position, Rotation rotation) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate#getJigsaws");
    }
    public ObjectArrayList<StructureBlockInfo> filterBlocks(BlockPos position, StructurePlaceSettings settings, Block block, boolean absolute) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate#filterBlocks");
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
        return org.basaltmc.bukkit.LeftoverNatives.structurePlace(org.basaltmc.bukkit.facade.NbtCodec.toBytes(data),
            server.getWorld().getName(), position.getX(), position.getY(), position.getZ(), settings.getRotation().ordinal(),
            settings.getMirror().ordinal(), 1.0F, settings.getRandom(position).nextLong(), !settings.isIgnoreEntities(), 0);
    }
    public static void updateShapeAtEdge(LevelAccessor level, int updateMode, DiscreteVoxelShape shape, BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate#updateShapeAtEdge");
    }
    public static void updateShapeAtEdge(LevelAccessor level, int updateMode, DiscreteVoxelShape shape, int startX, int startY, int startZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate#updateShapeAtEdge");
    }
    public static List<StructureBlockInfo> processBlockInfos(ServerLevelAccessor level, BlockPos position, BlockPos referencePos, StructurePlaceSettings settings, List<StructureBlockInfo> blockInfoList) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate#processBlockInfos");
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

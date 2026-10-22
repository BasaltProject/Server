package net.minecraft.world.level.block;

import java.util.Iterator;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.IdMapper;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.material.MapColor;
import net.minecraft.world.phys.shapes.VoxelShape;
import org.basaltmc.bukkit.Registries;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.PropertyPool;

public class Block extends BlockBehaviour implements ItemLike {
    public static final IdMapper<BlockState> BLOCK_STATE_REGISTRY = new StateIds();
    private static volatile Block[] kBlocks;

    private final int index;
    private final Identifier key;
    private final int first;
    private final int count;
    private Property<?>[] properties;
    private int[] strides;
    private volatile BlockState[] states;
    private StateDefinition<Block, BlockState> definition;
    private double[] facts;
    private double[] info;

    protected Block(int index) {
        this.index = index;
        this.key = Identifier.parse(Registries.blockName(index));
        this.first = Registries.firstState(index);
        this.count = Registries.stateCount(index);
    }

    public static Block byIndex(int index) {
        return blocks()[index];
    }

    public static int blockCount() {
        return blocks().length;
    }

    private static Block[] blocks() {
        Block[] found = kBlocks;
        if (found == null) {
            synchronized (Block.class) {
                found = kBlocks;
                if (found == null) {
                    Block[] made = new Block[Registries.blockCount()];
                    for (int i = 0; i < made.length; i++) {
                        made[i] = (WorldNatives.stateFlags(Registries.firstState(i)) & (1 << 18)) != 0
                            ? new EntityBlockImpl(i) : new Block(i);
                    }
                    kBlocks = found = made;
                }
            }
        }
        return found;
    }

    private static final class EntityBlockImpl extends Block implements EntityBlock {
        private EntityBlockImpl(int index) {
            super(index);
        }
    }

    public int blockIndex() {
        return index;
    }

    public Identifier blockKey() {
        return key;
    }

    public int firstState() {
        return first;
    }

    private void loadProperties() {
        if (properties == null) {
            synchronized (this) {
                if (properties == null) {
                    Registries.Properties table = Registries.propertiesOf(index);
                    Property<?>[] made = new Property<?>[table.names().length];
                    for (int i = 0; i < made.length; i++) {
                        made[i] = PropertyPool.of(table.names()[i], table.values()[i]);
                    }
                    strides = table.strides();
                    properties = made;
                }
            }
        }
    }

    public Property<?>[] propertiesInOrder() {
        loadProperties();
        return properties;
    }

    public int strideOf(int propertyIndex) {
        loadProperties();
        return strides[propertyIndex];
    }

    public int indexOf(Property<?> property) {
        Property<?>[] all = propertiesInOrder();
        for (int i = 0; i < all.length; i++) {
            if (all[i] == property) {
                return i;
            }
        }
        return -1;
    }

    public BlockState stateAtOffset(int offset) {
        BlockState[] all = states();
        return offset >= 0 && offset < all.length ? all[offset] : null;
    }

    private BlockState[] states() {
        BlockState[] found = states;
        if (found == null) {
            synchronized (this) {
                found = states;
                if (found == null) {
                    BlockState[] made = new BlockState[count];
                    for (int i = 0; i < count; i++) {
                        made[i] = new BlockState(this, first + i);
                    }
                    states = found = made;
                }
            }
        }
        return found;
    }

    public double[] facts() {
        if (facts == null) {
            double[] found = WorldNatives.blockHardness(key.toString());
            facts = found == null || found.length < 3 ? new double[] {0.0, 0.0, 0.0} : found;
        }
        return facts;
    }

    public double[] info() {
        if (info == null) {
            double[] found = WorldNatives.blockInfo(first);
            info = found == null || found.length < 7 ? new double[] {0.6, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0} : found;
        }
        return info;
    }

    public BlockState defaultBlockState() {
        return stateById(Registries.defaultState(index));
    }

    public StateDefinition<Block, BlockState> getStateDefinition() {
        StateDefinition<Block, BlockState> found = definition;
        if (found == null) {
            found = new StateDefinition<>(this, List.of(propertiesInOrder()), List.of(states()));
            definition = found;
        }
        return found;
    }

    public static BlockState stateById(int idWithData) {
        int block = Registries.blockOfState(idWithData);
        if (block < 0) {
            return null;
        }
        Block owner = byIndex(block);
        return owner.stateAtOffset(idWithData - owner.first);
    }

    public static int getId(BlockState state) {
        return state == null ? -1 : state.stateId();
    }

    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return defaultBlockState();
    }

    public static BlockState updateFromNeighbourShapes(BlockState state, LevelAccessor level, BlockPos pos) {
        return state;
    }

    public static boolean isShapeFullBlock(VoxelShape shape) {
        return shape != null && shape.isFullBlock();
    }

    public MapColor defaultMapColor() {
        return new MapColor((int) info()[4]);
    }

    public String getDescriptionId() {
        return "block." + key.getNamespace() + "." + key.getPath().replace('/', '.');
    }

    public float getExplosionResistance() {
        return (float) facts()[1];
    }

    public float getFriction() {
        return (float) info()[0];
    }

    public float getSpeedFactor() {
        return (float) info()[1];
    }

    public float getJumpFactor() {
        return (float) info()[2];
    }

    @Override
    public String toString() {
        return "Block{" + key + "}";
    }

    private static final class StateIds extends IdMapper<BlockState> {
        @Override
        public int getId(BlockState state) {
            return state == null ? -1 : state.stateId();
        }

        @Override
        public BlockState byId(int id) {
            return stateById(id);
        }

        @Override
        public int size() {
            int last = Registries.blockCount() - 1;
            return Registries.firstState(last) + Registries.stateCount(last);
        }

        @Override
        public Iterator<BlockState> iterator() {
            return new Iterator<>() {
                private int next;

                @Override
                public boolean hasNext() {
                    return next < size();
                }

                @Override
                public BlockState next() {
                    return stateById(next++);
                }
            };
        }
    }
}

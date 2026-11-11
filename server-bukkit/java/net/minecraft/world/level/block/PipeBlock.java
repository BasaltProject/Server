// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public abstract class PipeBlock extends Block {
    public static final BooleanProperty NORTH = net.minecraft.world.level.block.state.properties.BlockStateProperties.NORTH;
    public static final BooleanProperty EAST = net.minecraft.world.level.block.state.properties.BlockStateProperties.EAST;
    public static final BooleanProperty SOUTH = net.minecraft.world.level.block.state.properties.BlockStateProperties.SOUTH;
    public static final BooleanProperty WEST = net.minecraft.world.level.block.state.properties.BlockStateProperties.WEST;
    public static final BooleanProperty UP = net.minecraft.world.level.block.state.properties.BlockStateProperties.UP;
    public static final BooleanProperty DOWN = net.minecraft.world.level.block.state.properties.BlockStateProperties.DOWN;
    public static final Map<Direction, BooleanProperty> PROPERTY_BY_DIRECTION = java.util.Collections.unmodifiableMap(propertyMap());
    private static Map<Direction, BooleanProperty> propertyMap() {
        Map<Direction, BooleanProperty> map = new java.util.EnumMap<>(Direction.class);
        map.put(Direction.NORTH, NORTH);
        map.put(Direction.EAST, EAST);
        map.put(Direction.SOUTH, SOUTH);
        map.put(Direction.WEST, WEST);
        map.put(Direction.UP, UP);
        map.put(Direction.DOWN, DOWN);
        return map;
    }
    protected PipeBlock(float size, BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected boolean propagatesSkylightDown(BlockState state) {
        return super.propagatesSkylightDown(state);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.level.block.state.properties.ChestType;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class CopperChestBlock extends ChestBlock {
    private WeatheringCopper.WeatherState weatherState = WeatheringCopper.WeatherState.UNAFFECTED;
    private static Direction partnerFacing(Level level, BlockPos pos, Direction side) {
        BlockState state = level.getBlockState(pos.relative(side));
        return state.is(net.minecraft.tags.BlockTags.COPPER_CHESTS) && state.hasProperty(ChestBlock.TYPE)
            && state.getValue(ChestBlock.TYPE) == ChestType.SINGLE ? state.getValue(FACING) : null;
    }
    private static ChestType chestType(Level level, BlockPos pos, Direction facing) {
        if (facing == partnerFacing(level, pos, facing.getClockWise())) {
            return ChestType.LEFT;
        }
        return facing == partnerFacing(level, pos, facing.getCounterClockWise()) ? ChestType.RIGHT : ChestType.SINGLE;
    }
    private static BlockState unwax(CopperChestBlock block, BlockState state) {
        if (!block.isWaxed()) {
            return state;
        }
        String path = block.blockKey().getPath();
        Block plain = path.startsWith("waxed_") ? Blocks.named("minecraft:" + path.substring(6)) : null;
        return plain == null ? state : plain.withPropertiesOf(state);
    }
    private static BlockState leastOxidized(BlockState state, Level level, BlockPos pos) {
        if (state.getValue(ChestBlock.TYPE) == ChestType.SINGLE || !(state.getBlock() instanceof CopperChestBlock own)) {
            return state;
        }
        BlockState connected = level.getBlockState(pos.relative(ChestBlock.getConnectedDirection(state)));
        if (!(connected.getBlock() instanceof CopperChestBlock other)) {
            return state;
        }
        BlockState ownState = state;
        BlockState otherState = connected;
        if (own.isWaxed() != other.isWaxed()) {
            ownState = unwax(own, state);
            otherState = unwax(other, connected);
        }
        Block least = own.weatherState.ordinal() <= other.weatherState.ordinal() ? ownState.getBlock() : otherState.getBlock();
        return least.withPropertiesOf(ownState);
    }
    public CopperChestBlock(WeatheringCopper.WeatherState weatherState, SoundEvent openSound, SoundEvent closeSound, BlockBehaviour.Properties properties) {
        super((int) 0);
        this.weatherState = weatherState;
    }
    public static SoundEvent getHingeSound(WeatheringCopper.WeatherState state, boolean open) {
        return switch (state) {
            case WEATHERED -> open ? SoundEvents.COPPER_CHEST_WEATHERED_OPEN : SoundEvents.COPPER_CHEST_WEATHERED_CLOSE;
            case OXIDIZED -> open ? SoundEvents.COPPER_CHEST_OXIDIZED_OPEN : SoundEvents.COPPER_CHEST_OXIDIZED_CLOSE;
            default -> open ? SoundEvents.COPPER_CHEST_OPEN : SoundEvents.COPPER_CHEST_CLOSE;
        };
    }
    public boolean chestCanConnectTo(BlockState blockState) {
        return blockState.is(net.minecraft.tags.BlockTags.COPPER_CHESTS) && blockState.hasProperty(ChestBlock.TYPE);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    public WeatheringCopper.WeatherState getState() {
        return weatherState;
    }
    public static BlockState getFromCopperBlock(Block copperBlock, Direction facing, Level level, BlockPos pos) {
        String path = copperBlock.blockKey().getPath();
        String chest = path.endsWith("_block") ? path.substring(0, path.length() - "_block".length()) + "_chest" : path + "_chest";
        Block found = Blocks.named("minecraft:" + chest);
        CopperChestBlock block = found instanceof CopperChestBlock typed ? typed : null;
        Block base = found != null ? found : Blocks.named("minecraft:copper_chest");
        ChestType type = chestType(level, pos, facing);
        BlockState state = base.defaultBlockState().setValue(FACING, facing).setValue(TYPE, type);
        return leastOxidized(state, level, pos);
    }
    public boolean isWaxed() {
        return true;
    }
    public boolean shouldChangedStateKeepBlockEntity(BlockState oldState) {
        return super.shouldChangedStateKeepBlockEntity(oldState);
    }
}

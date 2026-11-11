// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import net.minecraft.core.Direction;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.vibrations.VibrationSystem;
public class CalibratedSculkSensorBlockEntity extends SculkSensorBlockEntity {
    public CalibratedSculkSensorBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("calibrated_sculk_sensor"), worldPosition, blockState);
    }
    public CalibratedSculkSensorBlockEntity(net.minecraft.server.level.ServerLevel level, net.minecraft.core.BlockPos pos, net.minecraft.nbt.CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public VibrationSystem.User createVibrationUser() {
        return new VibrationUser(getBlockPos());
    }
    protected class VibrationUser extends SculkSensorBlockEntity.VibrationUser {
        public VibrationUser(BlockPos blockPos) {
            super(blockPos);
        }
        public int getListenerRadius() {
            return 16;
        }
        public boolean canReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, GameEvent. Context context) {
            Direction behind = getBlockState().getValue(BlockStateProperties.HORIZONTAL_FACING).getOpposite();
            int expected = level.getSignal(blockPos.relative(behind), behind);
            if (expected != 0 && VibrationSystem.getGameEventFrequency(event) != expected) {
                return false;
            }
            return super.canReceiveVibration(level, pos, event, context);
        }
    }
}

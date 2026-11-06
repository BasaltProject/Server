// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.Holder;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.GameEventListener;
import net.minecraft.world.level.gameevent.PositionSource;
import net.minecraft.world.level.gameevent.vibrations.VibrationSystem;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class SculkSensorBlockEntity extends BlockEntity {
    public int getListenerRange() {
        org.bukkit.block.SculkSensor sensor = bukkit(org.bukkit.block.SculkSensor.class);
        return sensor == null ? 8 : sensor.getListenerRange();
    }

    public SculkSensorBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("sculk_sensor"), worldPosition, blockState);
    }

    public SculkSensorBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public int getLastVibrationFrequency() {
        return nbt().getIntOr("last_vibration_frequency", 0);
    }

    protected class VibrationUser implements VibrationSystem.User {
        public static final int LISTENER_RANGE = 8;
        protected final BlockPos blockPos;
        public VibrationUser(BlockPos blockPos) {
            this.blockPos = blockPos;
        }
        public int getListenerRadius() {
            return getListenerRange();
        }
        public PositionSource getPositionSource() {
            return new net.minecraft.world.level.gameevent.BlockPositionSource(blockPos);
        }
        public boolean canTriggerAvoidVibration() {
            return true;
        }
        public boolean canReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, GameEvent. Context context) {
            return !pos.equals(blockPos);
        }
        public void onReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, Entity sourceEntity, Entity projectileOwner, float receivingDistance) {
        }
        public void onDataChanged() {
            setChanged();
        }
        public boolean requiresAdjacentChunksToBeTicking() {
            return true;
        }
    }
}

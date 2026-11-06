// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.Holder;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.SculkSpreader;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.GameEventListener;
import net.minecraft.world.level.gameevent.PositionSource;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class SculkCatalystBlockEntity extends BlockEntity {
    public void bloom(int charge) {
        org.bukkit.block.SculkCatalyst catalyst = bukkit(org.bukkit.block.SculkCatalyst.class);
        if (catalyst != null) {
            catalyst.bloom(getLevel() instanceof ServerLevel level ? level.getWorld().getBlockAt(getBlockPos().getX(), getBlockPos().getY(), getBlockPos().getZ()) : null, charge);
        }
    }

    public SculkCatalystBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("sculk_catalyst"), worldPosition, blockState);
    }

    public SculkCatalystBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public static class CatalystListener implements GameEventListener {
        public static final int PULSE_TICKS = 8;
        private final PositionSource positionSource;
        private final SculkSpreader spreader = SculkSpreader.createLevelSpreader();
        public CatalystListener(BlockState blockState, PositionSource positionSource) {
            this.positionSource = positionSource;
        }
        public PositionSource getListenerSource() {
            return positionSource;
        }
        public int getListenerRadius() {
            return 8;
        }
        public GameEventListener.DeliveryMode getDeliveryMode() {
            return GameEventListener.DeliveryMode.BY_DISTANCE;
        }
        public boolean handleGameEvent(ServerLevel level, Holder<GameEvent> event, GameEvent.Context context, Vec3 sourcePosition) {
            // The native catalyst system reacts to ENTITY_DIE.
            return false;
        }
        public SculkSpreader getSculkSpreader() {
            return spreader;
        }
    }
}

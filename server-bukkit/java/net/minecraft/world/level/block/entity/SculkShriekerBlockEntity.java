// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.tags.TagKey;
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

public class SculkShriekerBlockEntity extends BlockEntity {
    public SculkShriekerBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("sculk_shrieker"), worldPosition, blockState);
    }

    public SculkShriekerBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public boolean tryShriek(ServerLevel level, ServerPlayer player) {
        org.bukkit.block.SculkShrieker shrieker = bukkit(org.bukkit.block.SculkShrieker.class);
        if (shrieker == null || player == null || !(player.getBukkitEntity() instanceof org.bukkit.entity.Player bukkitPlayer)) {
            return false;
        }
        shrieker.tryShriek(bukkitPlayer);
        return true;
    }

    public boolean canRespond() {
        return getBlockState().getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.CAN_SUMMON);
    }

    class VibrationUser implements VibrationSystem.User {
        public VibrationUser() {
            
        }
        public int getListenerRadius() {
            return 8;
        }
        public PositionSource getPositionSource() {
            return new net.minecraft.world.level.gameevent.BlockPositionSource(getBlockPos());
        }
        public TagKey<GameEvent> getListenableEvents() {
            return net.minecraft.tags.GameEventTags.SHRIEKER_CAN_LISTEN;
        }
        public boolean canReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, GameEvent.Context context) {
            return !pos.equals(getBlockPos());
        }
        public void onReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, Entity sourceEntity, Entity projectileOwner, float receivingDistance) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.SculkShriekerBlockEntity.VibrationUser#onReceiveVibration");
        }
        public void onDataChanged() {
            setChanged();
        }
        public boolean requiresAdjacentChunksToBeTicking() {
            return true;
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

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

public class TrialSpawnerBlockEntity extends BlockEntity {
    public TrialSpawnerBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("trial_spawner"), worldPosition, blockState);
    }

    public TrialSpawnerBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public boolean isOminous() {
        return getBlockState().getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.OMINOUS);
    }

    public java.util.Collection<org.bukkit.entity.Player> getTrackedPlayers() {
        org.bukkit.block.TrialSpawner spawner = bukkit(org.bukkit.block.TrialSpawner.class);
        return spawner == null ? java.util.List.of() : spawner.getTrackedPlayers();
    }

    public int getRequiredPlayerRange() {
        org.bukkit.block.TrialSpawner spawner = bukkit(org.bukkit.block.TrialSpawner.class);
        return spawner == null ? 14 : spawner.getRequiredPlayerRange();
    }

    public int getCooldownLength() {
        org.bukkit.block.TrialSpawner spawner = bukkit(org.bukkit.block.TrialSpawner.class);
        return spawner == null ? 0 : spawner.getCooldownLength();
    }
}

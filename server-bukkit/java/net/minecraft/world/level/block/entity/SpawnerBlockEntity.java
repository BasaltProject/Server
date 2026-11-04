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

public class SpawnerBlockEntity extends BlockEntity {
    public SpawnerBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("mob_spawner"), worldPosition, blockState);
    }

    public SpawnerBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public int getDelay() {
        return nbt().getShortOr("Delay", (short) 20);
    }

    public int getRequiredPlayerRange() {
        return nbt().getShortOr("RequiredPlayerRange", (short) 16);
    }

    public int getSpawnCount() {
        return nbt().getShortOr("SpawnCount", (short) 4);
    }

    public int getMaxNearbyEntities() {
        return nbt().getShortOr("MaxNearbyEntities", (short) 6);
    }

    public void setEntityId(net.minecraft.world.entity.EntityType<?> type, net.minecraft.util.RandomSource random) {
        CompoundTag entity = new CompoundTag();
        entity.putString("id", net.minecraft.world.entity.EntityType.getKey(type).toString());
        CompoundTag data = new CompoundTag();
        data.put("entity", entity);
        CompoundTag tag = nbt();
        tag.put("SpawnData", data);
        tag.remove("SpawnPotentials");
        load(tag);
    }
}

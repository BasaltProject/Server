// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.vault;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.state.BlockState;

public class VaultBlockEntity extends BlockEntity {
    public VaultBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("vault"), worldPosition, blockState);
    }

    public VaultBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public ItemStack getDisplayItem() {
        return ItemStack.OPTIONAL_CODEC.parse(net.minecraft.nbt.NbtOps.INSTANCE, nbt().getCompoundOrEmpty("shared_data").getCompoundOrEmpty("display_item"))
            .result().orElse(ItemStack.EMPTY);
    }

    public CompoundTag getConfig() {
        return nbt().getCompoundOrEmpty("config");
    }

    public java.util.Set<java.util.UUID> getRewardedPlayers() {
        java.util.Set<java.util.UUID> players = new java.util.HashSet<>();
        for (net.minecraft.nbt.Tag entry : nbt().getCompoundOrEmpty("server_data").getListOrEmpty("rewarded_players")) {
            if (entry instanceof net.minecraft.nbt.IntArrayTag array && array.getAsIntArray().length == 4) {
                int[] words = array.getAsIntArray();
                players.add(new java.util.UUID((long) words[0] << 32 | (words[1] & 0xFFFFFFFFL), (long) words[2] << 32 | (words[3] & 0xFFFFFFFFL)));
            }
        }
        return players;
    }

    public boolean hasRewarded(java.util.UUID player) {
        return getRewardedPlayers().contains(player);
    }

    public boolean isOminous() {
        return getBlockState().getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.OMINOUS);
    }
}

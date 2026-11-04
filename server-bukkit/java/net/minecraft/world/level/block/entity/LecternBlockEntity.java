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

public class LecternBlockEntity extends BlockEntity implements net.minecraft.world.Clearable, net.minecraft.world.MenuProvider {
    public LecternBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("lectern"), worldPosition, blockState);
    }

    public LecternBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public ItemStack getBook() {
        return net.minecraft.world.item.ItemStack.OPTIONAL_CODEC.parse(net.minecraft.nbt.NbtOps.INSTANCE, nbt().getCompoundOrEmpty("Book"))
            .result().orElse(ItemStack.EMPTY);
    }

    public boolean hasBook() {
        return !getBook().isEmpty();
    }

    public int getPage() {
        return nbt().getIntOr("Page", 0);
    }

    @Override
    public void clearContent() {
        TileNbt.update(this, tag -> {
            tag.remove("Book");
            tag.remove("Page");
        });
    }

    @Override
    public Component getDisplayName() {
        return Component.translatable("container.lectern");
    }

    @Override
    public AbstractContainerMenu createMenu(int containerId, Inventory inventory, Player player) {
        return new net.minecraft.world.inventory.LecternMenu(containerId, new net.minecraft.world.SimpleContainer(1),
            new net.minecraft.world.inventory.SimpleContainerData(1));
    }
}

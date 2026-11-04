// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class EnchantingTableBlockEntity extends BlockEntity implements net.minecraft.world.Nameable {
    public EnchantingTableBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("enchanting_table"), worldPosition, blockState);
    }

    public EnchantingTableBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    @Override
    public Component getCustomName() {
        return TileNbt.component(this, "CustomName");
    }

    public void setCustomName(Component name) {
        TileNbt.setComponent(this, "CustomName", name);
    }

    @Override
    public Component getName() {
        Component custom = getCustomName();
        return custom != null ? custom : Component.translatable("container.enchant");
    }

    @Override
    public Component getDisplayName() {
        return getName();
    }

    @Override
    public boolean hasCustomName() {
        return getCustomName() != null;
    }
}

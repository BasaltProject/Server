// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class BannerBlockEntity extends BlockEntity implements net.minecraft.world.Nameable {
    public BannerBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("banner"), worldPosition, blockState);
    }

    public BannerBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
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
        return custom != null ? custom : Component.translatable("block.minecraft.banner");
    }

    @Override
    public Component getDisplayName() {
        return getName();
    }

    @Override
    public boolean hasCustomName() {
        return getCustomName() != null;
    }

    public net.minecraft.world.level.block.entity.BannerPatternLayers getPatterns() {
        net.minecraft.nbt.Tag stored = nbt().get("patterns");
        return stored == null ? net.minecraft.world.level.block.entity.BannerPatternLayers.EMPTY
            : net.minecraft.core.component.DataComponents.BANNER_PATTERNS.codecOrThrow().parse(net.minecraft.nbt.NbtOps.INSTANCE, stored)
                .result().orElse(net.minecraft.world.level.block.entity.BannerPatternLayers.EMPTY);
    }
}

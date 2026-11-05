// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.Level;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.ItemStack;
import java.util.List;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class BeaconBlockEntity extends BlockEntity implements net.minecraft.world.MenuProvider, net.minecraft.world.Nameable, BeaconBeamOwner {
    public static final int DATA_LEVELS = 0;
    public static final int DATA_PRIMARY = 1;
    public static final int DATA_SECONDARY = 2;
    public static final int NUM_DATA_VALUES = 3;

    public BeaconBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("beacon"), worldPosition, blockState);
    }

    public BeaconBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public int getLevels() {
        return nbt().getIntOr("Levels", 0);
    }

    public net.minecraft.resources.Identifier getPrimaryEffectId() {
        String id = nbt().getStringOr("primary_effect", "");
        return id.isEmpty() ? null : net.minecraft.resources.Identifier.parse(id);
    }

    public net.minecraft.resources.Identifier getSecondaryEffectId() {
        String id = nbt().getStringOr("secondary_effect", "");
        return id.isEmpty() ? null : net.minecraft.resources.Identifier.parse(id);
    }

    @Override
    public List<BeaconBeamOwner.Section> getBeamSections() {
        if (getLevel() == null || getLevel().getWorld() == null) {
            return List.of();
        }
        int[] scan = org.basaltmc.bukkit.MechanicsNatives.beaconScan(getLevel().getWorld().getName(), getBlockPos().getX(),
            getBlockPos().getY(), getBlockPos().getZ());
        List<BeaconBeamOwner.Section> sections = new java.util.ArrayList<>();
        for (int at = 1; at + 1 < scan.length; at += 2) {
            BeaconBeamOwner.Section section = new BeaconBeamOwner.Section(scan[at]);
            for (int height = 1; height < scan[at + 1]; height++) {
                section.increaseHeight();
            }
            sections.add(section);
        }
        return sections;
    }

    public static int updateBase(Level level, int x, int y, int z) {
        int[] scan = org.basaltmc.bukkit.MechanicsNatives.beaconScan(level.getWorld().getName(), x, y, z);
        return scan.length == 0 ? 0 : scan[0];
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
        return custom != null ? custom : Component.translatable("block.minecraft.beacon");
    }

    @Override
    public Component getDisplayName() {
        return getName();
    }

    @Override
    public AbstractContainerMenu createMenu(int containerId, Inventory inventory, Player player) {
        return new net.minecraft.world.inventory.BeaconMenu(containerId, inventory, new net.minecraft.world.inventory.SimpleContainerData(3), net.minecraft.world.inventory.ContainerLevelAccess.create(getLevel(), getBlockPos()));
    }
}

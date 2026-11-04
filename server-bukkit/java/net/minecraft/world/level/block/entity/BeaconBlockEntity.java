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
        org.bukkit.World world = getLevel().getWorld();
        List<BeaconBeamOwner.Section> sections = new java.util.ArrayList<>();
        BeaconBeamOwner.Section current = null;
        for (int y = getBlockPos().getY() + 1; y < world.getMaxHeight(); y++) {
            org.bukkit.Material type = world.getBlockAt(getBlockPos().getX(), y, getBlockPos().getZ()).getType();
            String name = type.name();
            if (name.endsWith("_STAINED_GLASS") || name.endsWith("_STAINED_GLASS_PANE")) {
                int rgb = net.minecraft.world.item.DyeColor.byName(
                    name.substring(0, name.indexOf("_STAINED")).toLowerCase(java.util.Locale.ROOT), net.minecraft.world.item.DyeColor.WHITE)
                    .getTextureDiffuseColor() & 0xFFFFFF;
                if (current == null) {
                    current = new BeaconBeamOwner.Section(rgb);
                    sections.add(current);
                } else {
                    int mixed = 0;
                    for (int shift = 0; shift <= 16; shift += 8) {
                        mixed |= ((((current.getColor() >> shift) & 0xFF) + ((rgb >> shift) & 0xFF)) / 2) << shift;
                    }
                    current = new BeaconBeamOwner.Section(mixed);
                    sections.add(current);
                }
            } else if (type != org.bukkit.Material.BEDROCK && type.isOccluding()) {
                break;
            } else {
                if (current == null) {
                    current = new BeaconBeamOwner.Section(0xFFFFFF);
                    sections.add(current);
                } else {
                    current.increaseHeight();
                }
            }
        }
        return sections;
    }

    public static int updateBase(Level level, int x, int y, int z) {
        org.bukkit.World world = level.getWorld();
        int levels = 0;
        for (int layer = 1; layer <= 4; layer++) {
            int by = y - layer;
            if (by < level.getMinY()) {
                break;
            }
            for (int bx = x - layer; bx <= x + layer; bx++) {
                for (int bz = z - layer; bz <= z + layer; bz++) {
                    if (!org.bukkit.Tag.BEACON_BASE_BLOCKS.isTagged(world.getBlockAt(bx, by, bz).getType())) {
                        return levels;
                    }
                }
            }
            levels = layer;
        }
        return levels;
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

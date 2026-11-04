// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle;
import net.minecraft.world.level.block.Block;
import net.minecraft.core.NonNullList;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.Container;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.MenuProvider;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public interface ContainerEntity extends Container, MenuProvider {
    Vec3 position();
    AABB getBoundingBox();
    ResourceKey<LootTable> getContainerLootTable();
    void setContainerLootTable(ResourceKey<LootTable> lootTable);
    long getContainerLootTableSeed();
    void setContainerLootTableSeed(long lootTableSeed);
    NonNullList<ItemStack> getItemStacks();
    void clearItemStacks();
    Level level();
    boolean isRemoved();
    default boolean isEmpty() {
        return isChestVehicleEmpty();
    }
    default void addChestVehicleSaveData(ValueOutput output) {

    }
    default void readChestVehicleSaveData(ValueInput input) {

    }
    default void chestVehicleDestroyed(DamageSource source, ServerLevel level, Entity entity) {
        if (level instanceof ServerLevel serverLevel) {
            for (ItemStack stack : getItemStacks()) {
                if (!stack.isEmpty()) {
                    Block.popResource(serverLevel, net.minecraft.core.BlockPos.containing(position()), stack);
                }
            }
            clearItemStacks();
        }
    }
    default InteractionResult interactWithContainerVehicle(Player player) {
        if (!player.isSpectator()) {
            player.openMenu(this);
        }
        return InteractionResult.SUCCESS;
    }
    default void unpackChestVehicleLootTable(Player player) {

    }
    default void clearChestVehicleContent() {
        clearItemStacks();
    }
    default boolean isChestVehicleEmpty() {
        for (ItemStack stack : getItemStacks()) {
            if (!stack.isEmpty()) {
                return false;
            }
        }
        return true;
    }
    default ItemStack removeChestVehicleItemNoUpdate(int slot) {
        ItemStack held = getItemStacks().get(slot);
        getItemStacks().set(slot, ItemStack.EMPTY);
        return held;
    }
    default ItemStack getChestVehicleItem(int slot) {
        return getItemStacks().get(slot);
    }
    default ItemStack removeChestVehicleItem(int slot, int count) {
        ItemStack held = getItemStacks().get(slot);
        return held.isEmpty() || count <= 0 ? ItemStack.EMPTY : held.split(count);
    }
    default void setChestVehicleItem(int slot, ItemStack itemStack) {
        getItemStacks().set(slot, itemStack);
    }
    default SlotAccess getChestVehicleSlot(int slot) {
        return slot >= 0 && slot < getContainerSize() ? new SlotAccess() {
            public ItemStack get() {
                return getChestVehicleItem(slot);
            }

            public boolean set(ItemStack stack) {
                setChestVehicleItem(slot, stack);
                return true;
            }
        } : null;
    }
    default boolean isChestVehicleStillValid(Player player) {
        return !isRemoved() && position().closerThan(player.position(), 8.0);
    }
}

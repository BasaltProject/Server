// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.core.NonNullList;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.vehicle.ContainerEntity;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractMinecartContainer extends AbstractMinecart implements ContainerEntity {
    protected AbstractMinecartContainer(EntityType<?> type, Level level) {
        super(type, level);
    }
    public void destroy(ServerLevel level, DamageSource source) {
        super.destroy(level, source);
    }
    public ItemStack getItem(int slot) {
        org.bukkit.inventory.Inventory inventory = bukkitInventory();
        return inventory == null || slot < 0 || slot >= inventory.getSize() ? ItemStack.EMPTY
            : org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(inventory.getItem(slot));
    }
    public ItemStack removeItem(int slot, int count) {
        ItemStack held = getItem(slot);
        if (held.isEmpty() || count <= 0) {
            return ItemStack.EMPTY;
        }
        ItemStack taken = held.split(count);
        setItem(slot, held);
        return taken;
    }
    public ItemStack removeItemNoUpdate(int slot) {
        ItemStack held = getItem(slot);
        setItem(slot, ItemStack.EMPTY);
        return held;
    }
    public void setItem(int slot, ItemStack itemStack) {
        org.bukkit.inventory.Inventory inventory = bukkitInventory();
        if (inventory != null && slot >= 0 && slot < inventory.getSize()) {
            inventory.setItem(slot, org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        }
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    public void setChanged() {

    }
    public boolean stillValid(Player player) {
        return !isRemoved() && player.distanceToSqr(this) <= 64.0;
    }
    public void remove(Entity.RemovalReason reason) {
        super.remove(reason);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected Vec3 applyNaturalSlowdown(Vec3 deltaMovement) {
        float keep = 0.95F + (15 - AbstractContainerMenu.getRedstoneSignalFromContainer(this)) * 0.001F;
        return deltaMovement.multiply(keep, 0.0, keep);
    }
    public void clearContent() {
        org.bukkit.inventory.Inventory inventory = bukkitInventory();
        if (inventory != null) {
            inventory.clear();
        }
    }
    public void setLootTable(ResourceKey<LootTable> lootTable, long seed) {
        setContainerLootTable(lootTable);
        setContainerLootTableSeed(seed);
    }
    public AbstractContainerMenu createMenu(int containerId, Inventory inventory, Player player) {
        return createMenu(containerId, inventory);
    }
    protected abstract AbstractContainerMenu createMenu(int containerId, Inventory inventory);
    public ResourceKey<LootTable> getContainerLootTable() {
        return live() instanceof org.bukkit.loot.Lootable lootable && lootable.getLootTable() != null
            ? lootKey(lootable.getLootTable().getKey().toString()) : null;
    }
    public void setContainerLootTable(ResourceKey<LootTable> lootTable) {
        if (live() instanceof org.bukkit.loot.Lootable lootable) {
            lootable.setLootTable(lootTable == null ? null : org.bukkit.Bukkit.getLootTable(org.bukkit.NamespacedKey.fromString(lootTable.identifier().toString())));
        }
    }
    public long getContainerLootTableSeed() {
        return live() instanceof org.bukkit.loot.Lootable lootable ? lootable.getSeed() : 0L;
    }
    public void setContainerLootTableSeed(long lootTableSeed) {
        if (live() instanceof org.bukkit.loot.Lootable lootable) {
            lootable.setSeed(lootTableSeed);
        }
    }
    public NonNullList<ItemStack> getItemStacks() {
        int size = getContainerSize();
        NonNullList<ItemStack> snapshot = NonNullList.withSize(size, ItemStack.EMPTY);
        for (int slot = 0; slot < size; slot++) {
            snapshot.set(slot, getItem(slot));
        }
        return snapshot;
    }
    public void clearItemStacks() {
        clearContent();
    }
    private org.bukkit.inventory.Inventory bukkitInventory() {
        return live() instanceof org.bukkit.inventory.InventoryHolder holder ? holder.getInventory() : null;
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static ResourceKey<LootTable> lootKey(String id) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.LOOT_TABLE, net.minecraft.resources.Identifier.parse(id));
    }
}

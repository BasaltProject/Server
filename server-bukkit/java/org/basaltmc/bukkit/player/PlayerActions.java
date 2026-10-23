package org.basaltmc.bukkit.player;

import io.papermc.paper.entity.PlayerGiveResult;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Random;
import java.util.UUID;
import java.util.function.Consumer;
import org.basaltmc.bukkit.Items;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.block.Block;
import org.bukkit.conversations.Conversation;
import org.bukkit.conversations.ConversationAbandonedEvent;
import org.bukkit.entity.Item;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.InventoryView;
import org.bukkit.inventory.ItemStack;
import org.bukkit.util.Vector;

public abstract class PlayerActions extends PlayerMisc {
    private static final Random kRandom = new Random();
    private static final int kEnderChestSize = 27;
    private static final int kPickupDelayTicks = 40;

    private final PlayerConversations conversations = new PlayerConversations();
    private Inventory enderChest;

    private UUID key() {
        return getUniqueId();
    }

    @Override
    public boolean isConversing() {
        return conversations.isConversing();
    }

    @Override
    public boolean beginConversation(Conversation conversation) {
        return conversations.begin(conversation);
    }

    @Override
    public void abandonConversation(Conversation conversation) {
        conversations.abandon(conversation, new ConversationAbandonedEvent(conversation));
    }

    @Override
    public void abandonConversation(Conversation conversation, ConversationAbandonedEvent details) {
        conversations.abandon(conversation, details);
    }

    @Override
    public void acceptConversationInput(String input) {
        conversations.accept(input);
    }

    @Override
    public PlayerGiveResult give(Collection<ItemStack> items, boolean dropRemainder) {
        ItemStack[] given = items.toArray(new ItemStack[0]);
        List<ItemStack> leftovers = new ArrayList<>(getInventory().addItem(given).values());
        List<Item> drops = new ArrayList<>();
        if (dropRemainder) {
            for (ItemStack left : leftovers) {
                drops.add(throwOut(left, true, null));
            }
            leftovers = List.of();
        }
        List<ItemStack> kept = leftovers;
        return new PlayerGiveResult() {
            @Override
            public Collection<ItemStack> leftovers() {
                return kept;
            }

            @Override
            public Collection<Item> drops() {
                return drops;
            }
        };
    }

    @Override
    public void setItemInHand(ItemStack item) {
        getInventory().setItemInMainHand(item);
    }

    private Item throwOut(ItemStack stack, boolean randomly, Consumer<Item> configure) {
        Location at = getLocation();
        at.setY(getEyeLocation().getY() - 0.3);
        double vx;
        double vy;
        double vz;
        if (randomly) {
            double power = kRandom.nextFloat() * 0.5F;
            double angle = kRandom.nextFloat() * (Math.PI * 2);
            vx = -Math.sin(angle) * power;
            vy = 0.2;
            vz = Math.cos(angle) * power;
        } else {
            double pitch = Math.toRadians(getPitch());
            double yaw = Math.toRadians(getYaw());
            vx = -Math.sin(yaw) * Math.cos(pitch) * 0.3;
            vy = -Math.sin(pitch) * 0.3 + 0.1;
            vz = Math.cos(yaw) * Math.cos(pitch) * 0.3;
            double angle = kRandom.nextFloat() * (Math.PI * 2);
            double spread = 0.02 * kRandom.nextFloat();
            vx += Math.cos(angle) * spread;
            vy += (kRandom.nextFloat() - kRandom.nextFloat()) * 0.1;
            vz += Math.sin(angle) * spread;
        }
        Vector velocity = new Vector(vx, vy, vz);
        return getWorld().dropItem(at, stack, item -> {
            item.setVelocity(velocity);
            item.setPickupDelay(kPickupDelayTicks);
            item.setThrower(getUniqueId());
            if (configure != null) {
                configure.accept(item);
            }
        });
    }

    @Override
    public boolean dropItem(boolean dropAll) {
        ItemStack held = getInventory().getItemInMainHand();
        if (held == null || held.getType().isAir() || held.getAmount() <= 0) {
            return false;
        }
        int count = dropAll ? held.getAmount() : 1;
        ItemStack dropped = held.clone();
        dropped.setAmount(count);
        if (count >= held.getAmount()) {
            getInventory().setItemInMainHand(null);
        } else {
            held.setAmount(held.getAmount() - count);
            getInventory().setItemInMainHand(held);
        }
        throwOut(dropped, false, null);
        return true;
    }

    @Override
    public Item dropItem(ItemStack item, boolean randomly, Consumer<Item> configure) {
        return throwOut(item, randomly, configure);
    }

    @Override
    public Item dropItem(int slot, int amount, boolean randomly, Consumer<Item> configure) {
        ItemStack held = getInventory().getItem(slot);
        if (held == null || held.getType().isAir() || amount <= 0) {
            return null;
        }
        int count = Math.min(amount, held.getAmount());
        ItemStack dropped = held.clone();
        dropped.setAmount(count);
        if (count >= held.getAmount()) {
            getInventory().setItem(slot, null);
        } else {
            held.setAmount(held.getAmount() - count);
            getInventory().setItem(slot, held);
        }
        return throwOut(dropped, randomly, configure);
    }

    @Override
    public Item dropItem(EquipmentSlot slot, int amount, boolean randomly, Consumer<Item> configure) {
        ItemStack held = getInventory().getItem(slot);
        if (held == null || held.getType().isAir() || amount <= 0) {
            return null;
        }
        int count = Math.min(amount, held.getAmount());
        ItemStack dropped = held.clone();
        dropped.setAmount(count);
        if (count >= held.getAmount()) {
            getInventory().setItem(slot, null);
        } else {
            held.setAmount(held.getAmount() - count);
            getInventory().setItem(slot, held);
        }
        return throwOut(dropped, randomly, configure);
    }

    @Override
    public Inventory getEnderChest() {
        if (enderChest == null) {
            enderChest = new PlayerEnderChest(this);
        }
        return enderChest;
    }

    private InventoryView openTable(Location at, boolean force, InventoryType type, Material... blocks) {
        Location where = at == null ? getLocation() : at;
        if (!force) {
            Block block = where.getBlock();
            boolean fits = false;
            for (Material material : blocks) {
                fits |= block.getType() == material;
            }
            if (!fits) {
                return null;
            }
        }
        return openInventory(Bukkit.createInventory(this, type));
    }

    @Override
    public InventoryView openWorkbench(Location location, boolean force) {
        return openTable(location, force, InventoryType.WORKBENCH, Material.CRAFTING_TABLE);
    }

    @Override
    public InventoryView openAnvil(Location location, boolean force) {
        return openTable(location, force, InventoryType.ANVIL, Material.ANVIL, Material.CHIPPED_ANVIL,
            Material.DAMAGED_ANVIL);
    }

    @Override
    public InventoryView openEnchanting(Location location, boolean force) {
        return openTable(location, force, InventoryType.ENCHANTING, Material.ENCHANTING_TABLE);
    }

    @Override
    public InventoryView openGrindstone(Location location, boolean force) {
        return openTable(location, force, InventoryType.GRINDSTONE, Material.GRINDSTONE);
    }

    @Override
    public InventoryView openLoom(Location location, boolean force) {
        return openTable(location, force, InventoryType.LOOM, Material.LOOM);
    }

    @Override
    public InventoryView openSmithingTable(Location location, boolean force) {
        return openTable(location, force, InventoryType.SMITHING, Material.SMITHING_TABLE);
    }

    @Override
    public InventoryView openStonecutter(Location location, boolean force) {
        return openTable(location, force, InventoryType.STONECUTTER, Material.STONECUTTER);
    }

    @Override
    public InventoryView openCartographyTable(Location location, boolean force) {
        return openTable(location, force, InventoryType.CARTOGRAPHY, Material.CARTOGRAPHY_TABLE);
    }

    @Override
    public io.papermc.paper.entity.PlayerPostEffects postEffects() {
        return new PlayerPostEffects(this);
    }

    static ItemStack stack(Object bytes) {
        return bytes instanceof byte[] held ? Items.fromBytes(held) : null;
    }

    static int enderChestSize() {
        return kEnderChestSize;
    }
}

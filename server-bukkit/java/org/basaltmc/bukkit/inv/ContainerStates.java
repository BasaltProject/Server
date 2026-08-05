package org.basaltmc.bukkit.inv;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Map;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Material;
import org.bukkit.block.BlockState;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.Inventory;

public final class ContainerStates {
    private ContainerStates() {}

    private static Class<? extends BlockState> interfaceOf(String block) {
        switch (block) {
            case "chest", "trapped_chest":
                return org.bukkit.block.Chest.class;
            case "barrel":
                return org.bukkit.block.Barrel.class;
            case "hopper":
                return org.bukkit.block.Hopper.class;
            case "dispenser":
                return org.bukkit.block.Dispenser.class;
            case "dropper":
                return org.bukkit.block.Dropper.class;
            case "furnace":
                return org.bukkit.block.Furnace.class;
            case "blast_furnace":
                return org.bukkit.block.BlastFurnace.class;
            case "smoker":
                return org.bukkit.block.Smoker.class;
            case "brewing_stand":
                return org.bukkit.block.BrewingStand.class;
            case "crafter":
                return org.bukkit.block.Crafter.class;
            default:
                return block.endsWith("shulker_box") ? org.bukkit.block.ShulkerBox.class : null;
        }
    }

    private static InventoryType typeOf(String block) {
        switch (block) {
            case "chest", "trapped_chest":
                return InventoryType.CHEST;
            case "barrel":
                return InventoryType.BARREL;
            case "hopper":
                return InventoryType.HOPPER;
            case "dispenser":
                return InventoryType.DISPENSER;
            case "dropper":
                return InventoryType.DROPPER;
            case "furnace":
                return InventoryType.FURNACE;
            case "blast_furnace":
                return InventoryType.BLAST_FURNACE;
            case "smoker":
                return InventoryType.SMOKER;
            case "brewing_stand":
                return InventoryType.BREWING;
            case "crafter":
                return InventoryType.CRAFTER;
            default:
                return InventoryType.SHULKER_BOX;
        }
    }

    /** The state with the inventory accessors of its block, or the plain state for a block that holds no items. */
    public static BlockState of(String world, int x, int y, int z, BlockState base) {
        Material material = base.getType();
        String block = material.getKey().getKey();
        Class<? extends BlockState> iface = interfaceOf(block);
        if (iface == null) {
            return base;
        }
        Handler handler = new Handler(world, x, y, z, base, typeOf(block), block);
        BlockState proxy = (BlockState) Proxy.newProxyInstance(iface.getClassLoader(), new Class<?>[] {iface}, handler);
        handler.holder = (org.bukkit.inventory.InventoryHolder) proxy;
        return proxy;
    }

    /** The state of a container block that is not in the world, holding the stacks of the item it came from. */
    public static BlockState detached(BlockState base, Map<String, Object> data, java.util.List<Object> items) {
        String block = base.getType().getKey().getKey();
        Class<? extends BlockState> iface = interfaceOf(block);
        if (iface == null) {
            return base;
        }
        Handler handler = new Handler(null, 0, 0, 0, base, typeOf(block), block);
        handler.detachedData = data;
        InventoryType type = typeOf(block);
        org.bukkit.inventory.ItemStack[] slots = new org.bukkit.inventory.ItemStack[BlockInventory.sizeOf(block)];
        for (Object entry : items == null ? java.util.List.of() : items) {
            Map<String, Object> one = Nbt.map(entry);
            org.bukkit.inventory.ItemStack stack = one == null ? null : org.basaltmc.bukkit.item.Convert.stack(
                Nbt.map(one.get("item")));
            int slot = one == null ? -1 : Nbt.integer(one, "slot", -1);
            if (stack != null && slot >= 0 && slot < slots.length) {
                slots[slot] = stack;
            }
        }
        BlockState proxy = (BlockState) Proxy.newProxyInstance(iface.getClassLoader(), new Class<?>[] {iface}, handler);
        handler.holder = (org.bukkit.inventory.InventoryHolder) proxy;
        handler.fixed = TypedInventories.wrap(new SnapshotInventory(type, slots));
        return proxy;
    }

    /** The stacks of a detached container state in the item's own `container` format. */
    public static java.util.List<Object> detachedItems(BlockState state) {
        java.util.List<Object> out = new java.util.ArrayList<>();
        if (state != null && Proxy.isProxyClass(state.getClass())
            && Proxy.getInvocationHandler(state) instanceof Handler handler && handler.fixed != null) {
            Inventory inventory = TypedInventories.unwrap(handler.fixed);
            for (int slot = 0; slot < inventory.getSize(); slot++) {
                org.bukkit.inventory.ItemStack stack = inventory.getItem(slot);
                if (stack != null && !stack.getType().isAir()) {
                    Map<String, Object> entry = Nbt.compound();
                    entry.put("slot", slot);
                    entry.put("item", org.basaltmc.bukkit.item.Convert.stack(stack));
                    out.add(entry);
                }
            }
        }
        return out;
    }

    /** The block entity data a detached container state holds apart from its stacks, or null for another state. */
    public static Map<String, Object> detachedData(BlockState state) {
        if (state != null && Proxy.isProxyClass(state.getClass())
            && Proxy.getInvocationHandler(state) instanceof Handler handler && handler.fixed != null) {
            return handler.detachedData;
        }
        return null;
    }

    private static final class Handler implements InvocationHandler {
        private Inventory fixed;
        private Map<String, Object> detachedData;
        private final String world;
        private final int x;
        private final int y;
        private final int z;
        private final BlockState base;
        private final InventoryType type;
        private final String block;
        private org.bukkit.inventory.InventoryHolder holder;
        private Inventory inventory;

        Handler(String world, int x, int y, int z, BlockState base, InventoryType type, String block) {
            this.world = world;
            this.x = x;
            this.y = y;
            this.z = z;
            this.base = base;
            this.type = type;
            this.block = block;
        }

        private Inventory inventory() {
            if (fixed != null) {
                return fixed;
            }
            if (inventory == null) {
                Inventory single = new BlockInventory(world, x, y, z, type, BlockInventory.sizeOf(block), holder);
                inventory = TypedInventories.wrap(single);
            }
            return inventory;
        }

        private Map<String, Object> entity() {
            return detachedData != null ? detachedData : Nbt.readNamed(WorldNatives.blockEntityNbt(world, x, y, z));
        }

        private void store(Map<String, Object> data) {
            if (detachedData == null) {
                WorldNatives.setBlockEntityNbt(world, x, y, z, Nbt.writeNamed(data));
            }
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            String name = method.getName();
            Class<?> declaring = method.getDeclaringClass();
            if (declaring == Object.class) {
                switch (name) {
                    case "equals":
                        return proxy == args[0];
                    case "hashCode":
                        return base.hashCode();
                    default:
                        return "ContainerState" + base;
                }
            }
            switch (name) {
                case "getInventory", "getBlockInventory", "getSnapshotInventory":
                    if (args == null || args.length == 0) {
                        return inventory();
                    }
                    break;
                case "isSnapshot":
                    return true;
                case "getBurnTime":
                    return (short) Nbt.integer(entity(), "lit_time_remaining", 0);
                case "getCookTime":
                    return (short) Nbt.integer(entity(), "cooking_time_spent", 0);
                case "getCookTimeTotal":
                    return Nbt.integer(entity(), "cooking_total_time", 0);
                case "getBrewingTime":
                    return Nbt.integer(entity(), "BrewTime", 0);
                case "getFuelLevel":
                    return Nbt.integer(entity(), "Fuel", 0);
                case "setBurnTime", "setCookTime", "setCookTimeTotal", "setBrewingTime", "setFuelLevel": {
                    Map<String, Object> data = entity();
                    String key = name.equals("setBurnTime") ? "lit_time_remaining"
                        : name.equals("setCookTime") ? "cooking_time_spent"
                        : name.equals("setCookTimeTotal") ? "cooking_total_time"
                        : name.equals("setBrewingTime") ? "BrewTime" : "Fuel";
                    boolean wide = name.equals("setBrewingTime") || name.equals("setCookTimeTotal");
                    Number value = (Number) args[0];
                    data.put(key, wide ? (Object) value.intValue() : (Object) value.shortValue());
                    store(data);
                    return null;
                }
                case "getTransferCooldown":
                    return Nbt.integer(entity(), "TransferCooldown", 0);
                case "setTransferCooldown": {
                    Map<String, Object> data = entity();
                    data.put("TransferCooldown", args[0]);
                    store(data);
                    return null;
                }
                case "getColor":
                    return block.equals("shulker_box") ? null : org.bukkit.DyeColor.valueOf(block.substring(0, Math.max(0, block.length() - 11))
                        .toUpperCase(java.util.Locale.ROOT).replaceAll("_$", ""));
                case "isBlocked":
                    return false;
                case "isLocked":
                    return entity() != null && entity().containsKey("lock");
                default:
                    break;
            }
            if (declaring.isInstance(base)) {
                try {
                    return method.invoke(base, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                }
            }
            try {
                return base.getClass().getMethod(name, method.getParameterTypes()).invoke(base, args);
            } catch (NoSuchMethodException e) {
                // the base does not know the method either
            } catch (InvocationTargetException e) {
                throw e.getCause();
            }
            if (method.isDefault()) {
                return InvocationHandler.invokeDefault(proxy, method, args);
            }
            throw Unsupported.hit(declaring.getSimpleName() + "#" + name);
        }
    }
}

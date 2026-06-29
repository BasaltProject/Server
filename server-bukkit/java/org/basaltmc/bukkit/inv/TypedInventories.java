package org.basaltmc.bukkit.inv;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Map;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.AnvilInventory;
import org.bukkit.inventory.BeaconInventory;
import org.bukkit.inventory.BrewerInventory;
import org.bukkit.inventory.CartographyInventory;
import org.bukkit.inventory.CraftingInventory;
import org.bukkit.inventory.EnchantingInventory;
import org.bukkit.inventory.FurnaceInventory;
import org.bukkit.inventory.GrindstoneInventory;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.JukeboxInventory;
import org.bukkit.inventory.LecternInventory;
import org.bukkit.inventory.LoomInventory;
import org.bukkit.inventory.SmithingInventory;
import org.bukkit.inventory.StonecutterInventory;

/** Gives an inventory the typed accessors of its kind (fuel, result, matrix...) as named slots. */
public final class TypedInventories {
    private static final Map<String, Integer> kFurnace = Map.of("Result", 2, "Fuel", 1, "Smelting", 0);
    private static final Map<String, Integer> kBrewer = Map.of("Ingredient", 3, "Fuel", 4);
    private static final Map<String, Integer> kEnchanting = Map.of("", 0, "Secondary", 1);
    private static final Map<String, Integer> kBeacon = Map.of("", 0);
    private static final Map<String, Integer> kLectern = Map.of("Book", 0);
    private static final Map<String, Integer> kJukebox = Map.of("Record", 0);
    private static final Map<String, Integer> kAnvil = Map.of("FirstItem", 0, "SecondItem", 1, "Result", 2);
    private static final Map<String, Integer> kGrindstone = Map.of("UpperItem", 0, "LowerItem", 1, "Result", 2);
    private static final Map<String, Integer> kLoom = Map.of("Banner", 0, "Dye", 1, "Pattern", 2, "Result", 3);
    private static final Map<String, Integer> kSmithing = Map.of("InputTemplate", 0, "InputEquipment", 1,
        "InputMineral", 2, "Result", 3);
    private static final Map<String, Integer> kStonecutter = Map.of("InputItem", 0, "Result", 1);
    private static final Map<String, Integer> kCartography = Map.of("Result", 2);
    private static final Map<String, Integer> kWorkbench = Map.of("Result", 0);

    private TypedInventories() {}

    private static Class<? extends Inventory> interfaceOf(InventoryType type) {
        switch (type) {
            case FURNACE, BLAST_FURNACE, SMOKER:
                return FurnaceInventory.class;
            case BREWING:
                return BrewerInventory.class;
            case CRAFTING, WORKBENCH:
                return CraftingInventory.class;
            case ANVIL:
                return AnvilInventory.class;
            case ENCHANTING:
                return EnchantingInventory.class;
            case GRINDSTONE:
                return GrindstoneInventory.class;
            case LOOM:
                return LoomInventory.class;
            case SMITHING, SMITHING_NEW:
                return SmithingInventory.class;
            case STONECUTTER:
                return StonecutterInventory.class;
            case BEACON:
                return BeaconInventory.class;
            case LECTERN:
                return LecternInventory.class;
            case JUKEBOX:
                return JukeboxInventory.class;
            case CARTOGRAPHY:
                return CartographyInventory.class;
            default:
                return null;
        }
    }

    private static Map<String, Integer> slotsOf(InventoryType type) {
        switch (type) {
            case FURNACE, BLAST_FURNACE, SMOKER:
                return kFurnace;
            case BREWING:
                return kBrewer;
            case CRAFTING, WORKBENCH:
                return kWorkbench;
            case ANVIL:
                return kAnvil;
            case ENCHANTING:
                return kEnchanting;
            case GRINDSTONE:
                return kGrindstone;
            case LOOM:
                return kLoom;
            case SMITHING, SMITHING_NEW:
                return kSmithing;
            case STONECUTTER:
                return kStonecutter;
            case BEACON:
                return kBeacon;
            case LECTERN:
                return kLectern;
            case JUKEBOX:
                return kJukebox;
            case CARTOGRAPHY:
                return kCartography;
            default:
                return Map.of();
        }
    }

    public static Inventory wrap(Inventory base) {
        return wrap(base, null, Map.of());
    }

    /** As {@link #wrap(Inventory)}, with the recipe of a crafting or smithing inventory and the anvil's state. */
    public static Inventory wrap(Inventory base, org.bukkit.inventory.Recipe recipe, Map<String, Object> state) {
        Class<? extends Inventory> iface = interfaceOf(base.getType());
        if (iface == null) {
            return base;
        }
        return (Inventory) Proxy.newProxyInstance(iface.getClassLoader(), new Class<?>[] {iface},
            new Handler(base, slotsOf(base.getType()), recipe, state));
    }

    public static Inventory unwrap(Inventory inventory) {
        if (inventory != null && Proxy.isProxyClass(inventory.getClass())
            && Proxy.getInvocationHandler(inventory) instanceof Handler handler) {
            return handler.base;
        }
        return inventory;
    }

    private static final class Handler implements InvocationHandler {
        private final Inventory base;
        private final Map<String, Integer> slots;
        private final org.bukkit.inventory.Recipe recipe;
        private final Map<String, Object> state;

        Handler(Inventory base, Map<String, Integer> slots, org.bukkit.inventory.Recipe recipe,
                Map<String, Object> state) {
            this.base = base;
            this.slots = slots;
            this.recipe = recipe;
            this.state = state;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            Class<?> declaring = method.getDeclaringClass();
            String name = method.getName();
            if (declaring == Object.class) {
                switch (name) {
                    case "equals":
                        return proxy == args[0] || unwrap((Inventory) (args[0] instanceof Inventory other ? other : null)) == base;
                    case "hashCode":
                        return base.hashCode();
                    default:
                        return base.toString();
                }
            }
            if (declaring == Inventory.class || declaring == Iterable.class) {
                return call(method, args);
            }
            Object typed = typed(proxy, method, name, args);
            if (typed != kNone) {
                return typed;
            }
            if (method.isDefault()) {
                return InvocationHandler.invokeDefault(proxy, method, args);
            }
            if (name.equals("getHolder")) {
                Object holder = base.getHolder();
                return method.getReturnType().isInstance(holder) ? holder : null;
            }
            try {
                return call(Inventory.class.getMethod(name, method.getParameterTypes()), args);
            } catch (NoSuchMethodException notInventory) {
                throw Unsupported.hit(declaring.getSimpleName() + "#" + name);
            }
        }

        private Object call(Method method, Object[] args) throws Throwable {
            try {
                return method.invoke(base, args);
            } catch (InvocationTargetException e) {
                throw e.getCause();
            }
        }

        private static final Object kNone = new Object();

        private Object typed(Object proxy, Method method, String name, Object[] args) {
            int count = args == null ? 0 : args.length;
            if (name.equals("getMatrix") && count == 0) {
                int first = base.getType() == InventoryType.CRAFTING ? 1 : 1;
                int size = base.getSize() - first;
                ItemStack[] matrix = new ItemStack[size];
                for (int at = 0; at < size; at++) {
                    matrix[at] = base.getItem(first + at);
                }
                return matrix;
            }
            if (name.equals("setMatrix") && count == 1) {
                ItemStack[] matrix = (ItemStack[]) args[0];
                for (int at = 1; at < base.getSize(); at++) {
                    base.setItem(at, at - 1 < matrix.length ? matrix[at - 1] : null);
                }
                return null;
            }
            if (name.equals("getRecipe") && count == 0) {
                return recipe;
            }
            switch (name) {
                case "getRenameText":
                    return state.get("rename") instanceof String text && !text.isEmpty() ? text : null;
                case "getRepairCost", "getRepairCostAmount", "getMaximumRepairCost":
                    return state.getOrDefault(name.equals("getRepairCost") ? "cost"
                        : name.equals("getMaximumRepairCost") ? "maximum" : "amount", 0);
                default:
                    break;
            }
            if (name.equals("isFuel") && count == 1) {
                return args[0] != null && ((ItemStack) args[0]).getType().isFuel();
            }
            if (name.equals("canSmelt") && count == 1) {
                throw Unsupported.hit("FurnaceInventory#canSmelt");
            }
            if (name.startsWith("get") && count == 0 && slots.containsKey(name.substring(3))) {
                return base.getItem(slots.get(name.substring(3)));
            }
            if (name.startsWith("set") && count == 1 && args[0] instanceof ItemStack
                && slots.containsKey(name.substring(3))) {
                base.setItem(slots.get(name.substring(3)), (ItemStack) args[0]);
                return null;
            }
            return kNone;
        }
    }
}

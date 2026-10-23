package org.basaltmc.bukkit.inv;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.Keyed;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.InventoryView;
import org.bukkit.inventory.MenuType;
import org.bukkit.inventory.Merchant;
import org.bukkit.inventory.view.builder.LocationInventoryViewBuilder;
import org.bukkit.inventory.view.builder.MerchantInventoryViewBuilder;

/** The window kinds a plugin can ask for by menu type, and the views and builders that make them. */
public final class MenuTypes {
    private static final Map<String, BasaltMenuType> all = new LinkedHashMap<>();

    static {
        for (int rows = 1; rows <= 6; rows++) {
            add("generic_9x" + rows, InventoryType.CHEST, rows * 9, InventoryView.class);
        }
        add("generic_3x3", InventoryType.DISPENSER, 9, InventoryView.class);
        add("crafter_3x3", InventoryType.CRAFTER, 9, org.bukkit.inventory.view.CrafterView.class);
        add("anvil", InventoryType.ANVIL, 3, org.bukkit.inventory.view.AnvilView.class);
        add("beacon", InventoryType.BEACON, 1, org.bukkit.inventory.view.BeaconView.class);
        add("blast_furnace", InventoryType.BLAST_FURNACE, 3, org.bukkit.inventory.view.FurnaceView.class);
        add("brewing_stand", InventoryType.BREWING, 5, org.bukkit.inventory.view.BrewingStandView.class);
        add("crafting", InventoryType.WORKBENCH, 10, InventoryView.class);
        add("enchantment", InventoryType.ENCHANTING, 2, org.bukkit.inventory.view.EnchantmentView.class);
        add("furnace", InventoryType.FURNACE, 3, org.bukkit.inventory.view.FurnaceView.class);
        add("grindstone", InventoryType.GRINDSTONE, 3, InventoryView.class);
        add("hopper", InventoryType.HOPPER, 5, InventoryView.class);
        add("lectern", InventoryType.LECTERN, 1, org.bukkit.inventory.view.LecternView.class);
        add("loom", InventoryType.LOOM, 4, org.bukkit.inventory.view.LoomView.class);
        add("merchant", InventoryType.MERCHANT, 3, org.bukkit.inventory.view.MerchantView.class);
        add("shulker_box", InventoryType.SHULKER_BOX, 27, InventoryView.class);
        add("smithing", InventoryType.SMITHING, 4, InventoryView.class);
        add("smoker", InventoryType.SMOKER, 3, org.bukkit.inventory.view.FurnaceView.class);
        add("cartography_table", InventoryType.CARTOGRAPHY, 3, InventoryView.class);
        add("stonecutter", InventoryType.STONECUTTER, 2, org.bukkit.inventory.view.StonecutterView.class);
    }

    private MenuTypes() {}

    private static void add(String id, InventoryType type, int size, Class<? extends InventoryView> view) {
        all.put(id, new BasaltMenuType(NamespacedKey.minecraft(id), type, size, view));
    }

    public static List<Keyed> list() {
        return new ArrayList<>(all.values());
    }

    public static BasaltMenuType of(InventoryType type) {
        for (BasaltMenuType menu : all.values()) {
            if (menu.type == type) {
                return menu;
            }
        }
        return null;
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    public static final class BasaltMenuType implements MenuType.Typed {
        private final NamespacedKey key;
        final InventoryType type;
        private final int size;
        private final Class<? extends InventoryView> view;

        BasaltMenuType(NamespacedKey key, InventoryType type, int size, Class<? extends InventoryView> view) {
            this.key = key;
            this.type = type;
            this.size = size;
            this.view = view;
        }

        @Override
        public NamespacedKey getKey() {
            return key;
        }

        @Override
        public InventoryView create(HumanEntity player, String title) {
            return create(player, org.basaltmc.bukkit.item.Texts.fromLegacy(title));
        }

        @Override
        public InventoryView create(HumanEntity player, Component title) {
            Inventory top = new CustomInventory(null, type, size, title);
            top = TypedInventories.wrap(top);
            BasaltInventoryView base = new BasaltInventoryView(player, top, title == null ? type.defaultTitle() : title,
                false, -1);
            if (view == InventoryView.class) {
                return base;
            }
            if (view == org.bukkit.inventory.view.AnvilView.class) {
                return new PrepareViews.Anvil(player, top, base, PrepareViews.Anvil.state("", 0));
            }
            return (InventoryView) Proxy.newProxyInstance(view.getClassLoader(), new Class<?>[] {view},
                new ViewHandler(base));
        }

        @Override
        public MenuType.Typed typed() {
            return this;
        }

        @Override
        public MenuType.Typed typed(Class viewClass) {
            if (viewClass != view && !viewClass.isAssignableFrom(view)) {
                throw new IllegalArgumentException("Cannot cast menu type " + key + " to " + viewClass);
            }
            return this;
        }

        @Override
        public Class getInventoryViewClass() {
            return view;
        }

        @Override
        public org.bukkit.inventory.view.builder.InventoryViewBuilder builder() {
            if (type == InventoryType.MERCHANT) {
                return new MerchantBuilder(this, null, null);
            }
            return new LocationBuilder(this, null, null);
        }

        @Override
        public String toString() {
            return "MenuType[" + key + "]";
        }
    }

    /** A view of a typed window: the view calls go to the plain view, the typed accessors keep what they are told. */
    private static final class ViewHandler implements InvocationHandler {
        private final BasaltInventoryView base;
        private final Map<String, Object> state = new HashMap<>();

        ViewHandler(BasaltInventoryView base) {
            this.base = base;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            Class<?> declaring = method.getDeclaringClass();
            String name = method.getName();
            if (declaring == Object.class) {
                return name.equals("equals") ? proxy == args[0] : name.equals("hashCode") ? base.hashCode()
                    : "View" + base;
            }
            if (declaring == InventoryView.class) {
                try {
                    return method.invoke(base, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                }
            }
            if (name.equals("getTopInventory")) {
                return base.getTopInventory();
            }
            if (method.isDefault()) {
                return InvocationHandler.invokeDefault(proxy, method, args);
            }
            if (name.startsWith("set") && args != null && args.length >= 1) {
                state.put(name.substring(3), args.length == 1 ? args[0] : List.of(args));
                return null;
            }
            String property = name.startsWith("get") ? name.substring(3) : name.startsWith("is") ? name.substring(2) : name;
            if (state.containsKey(property)) {
                return state.get(property);
            }
            Class<?> returned = method.getReturnType();
            if (returned == boolean.class) {
                return false;
            }
            if (returned == int.class) {
                return 0;
            }
            if (returned == float.class) {
                return 0.0F;
            }
            if (returned == List.class) {
                return List.of();
            }
            if (returned == void.class || !returned.isPrimitive()) {
                return null;
            }
            throw Unsupported.hit(declaring.getSimpleName() + "#" + name);
        }
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    private static final class LocationBuilder implements LocationInventoryViewBuilder {
        private final BasaltMenuType menu;
        private Component title;
        private Location location;

        LocationBuilder(BasaltMenuType menu, Component title, Location location) {
            this.menu = menu;
            this.title = title;
            this.location = location;
        }

        @Override
        public LocationInventoryViewBuilder copy() {
            return new LocationBuilder(menu, title, location);
        }

        @Override
        public LocationInventoryViewBuilder title(Component title) {
            this.title = title;
            return this;
        }

        @Override
        public LocationInventoryViewBuilder checkReachable(boolean checkReachable) {
            return this;
        }

        @Override
        public LocationInventoryViewBuilder location(Location location) {
            this.location = location;
            return this;
        }

        @Override
        public InventoryView build(HumanEntity player) {
            return menu.create(player, title);
        }
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    private static final class MerchantBuilder implements MerchantInventoryViewBuilder {
        private final BasaltMenuType menu;
        private Component title;
        private Merchant merchant;

        MerchantBuilder(BasaltMenuType menu, Component title, Merchant merchant) {
            this.menu = menu;
            this.title = title;
            this.merchant = merchant;
        }

        @Override
        public MerchantInventoryViewBuilder copy() {
            return new MerchantBuilder(menu, title, merchant);
        }

        @Override
        public MerchantInventoryViewBuilder title(Component title) {
            this.title = title;
            return this;
        }

        @Override
        public MerchantInventoryViewBuilder merchant(Merchant merchant) {
            this.merchant = merchant;
            return this;
        }

        @Override
        public MerchantInventoryViewBuilder checkReachable(boolean checkReachable) {
            return this;
        }

        @Override
        public InventoryView build(HumanEntity player) {
            throw Unsupported.hit("MenuType.MERCHANT#build");
        }
    }
}

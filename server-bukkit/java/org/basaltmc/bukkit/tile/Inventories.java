package org.basaltmc.bukkit.tile;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Proxy;
import org.basaltmc.bukkit.inv.BlockInventory;
import org.basaltmc.bukkit.inv.TypedInventories;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

final class Inventories {
    private Inventories() {}

    static Inventory of(Tile tile, InventoryType type, int size, Class<? extends Inventory> face) {
        Inventory plain = TypedInventories.wrap(new BlockInventory(tile.world.getName(), tile.x, tile.y, tile.z, type,
            size, (InventoryHolder) tile.self));
        if (face == null || face.isInstance(plain)) {
            return plain;
        }
        return (Inventory) Proxy.newProxyInstance(face.getClassLoader(), new Class<?>[] {face}, (proxy, method, args) -> {
            if (method.getDeclaringClass() == Object.class) {
                return method.invoke(plain, args);
            }
            switch (method.getName()) {
                case "getHolder":
                    if (method.getParameterCount() == 0) {
                        return tile.self;
                    }
                    break;
                case "getItem":
                    if (method.getParameterCount() == 0) {
                        return plain.getItem(0);
                    }
                    break;
                case "setItem":
                    if (method.getParameterCount() == 1) {
                        plain.setItem(0, (ItemStack) args[0]);
                        return null;
                    }
                    break;
                default:
                    break;
            }
            try {
                return method.invoke(plain, args);
            } catch (InvocationTargetException e) {
                throw e.getCause();
            }
        });
    }
}

package org.basaltmc.bukkit.tile;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Map;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.persistence.BytesPdcStore;
import org.bukkit.Location;
import org.bukkit.block.BlockState;
import org.bukkit.craftbukkit.persistence.CraftPersistentDataContainer;

/** Block states of the blocks that own a block entity, each with the accessors of its kind over a copy of its data. */
public final class TileStates {
    private TileStates() {}

    /** The state with the accessors of its block's kind, or the plain state for a block that has none. */
    public static BlockState of(BlockState base) {
        if (!base.isPlaced()) {
            return base;
        }
        BlockState held = org.basaltmc.bukkit.inv.ContainerStates.of(base.getWorld().getName(), base.getX(),
            base.getY(), base.getZ(), base);
        TileKind kind = held != base ? Kinds.container(held) : Kinds.of(base.getType().getKey().getKey());
        if (kind == null) {
            return base;
        }
        Tile tile = new Tile(held, Tile.read(base.getWorld().getName(), base.getX(), base.getY(), base.getZ()));
        return proxy(tile, kind);
    }

    private static BlockState proxy(Tile tile, TileKind kind) {
        tile.self = (BlockState) Proxy.newProxyInstance(kind.face().getClassLoader(), new Class<?>[] {kind.face()},
            new Handler(tile, kind));
        return tile.self;
    }

    private static final class Handler implements InvocationHandler {
        private final Tile tile;
        private final TileKind kind;
        private CraftPersistentDataContainer container;

        Handler(Tile tile, TileKind kind) {
            this.tile = tile;
            this.kind = kind;
        }

        private CraftPersistentDataContainer container() {
            if (container == null) {
                container = new CraftPersistentDataContainer(new BytesPdcStore(
                    () -> Nbt.writeNamed(tile.mapOrCreate("PublicBukkitValues")),
                    bytes -> tile.put("PublicBukkitValues", Nbt.readNamed(bytes))));
            }
            return container;
        }

        private boolean update(boolean force, boolean physics) {
            if (!tile.base.update(force, physics)) {
                return false;
            }
            tile.write();
            return true;
        }

        private BlockState copy(BlockState moved) {
            Tile next = new Tile(moved, Nbt.deepCopy(tile.data));
            return proxy(next, kind);
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            String name = method.getName();
            Class<?> declaring = method.getDeclaringClass();
            if (declaring == Object.class) {
                return switch (name) {
                    case "equals" -> proxy == args[0];
                    case "hashCode" -> tile.base.hashCode();
                    default -> "TileState" + tile.base;
                };
            }
            int count = args == null ? 0 : args.length;
            switch (name) {
                case "update":
                    return update(count > 0 && (Boolean) args[0], count < 2 || (Boolean) args[1]);
                case "copy":
                    return copy(count == 0 ? tile.base.copy() : tile.base.copy((Location) args[0]));
                case "isSnapshot":
                    return true;
                case "getPersistentDataContainer":
                    return container();
                default:
                    break;
            }
            Object result = kind.call(tile, method, name, args);
            if (result == Tile.kNone) {
                result = Common.call(tile, name, args);
            }
            if (result != Tile.kNone) {
                return result;
            }
            if (method.isDefault()) {
                return InvocationHandler.invokeDefault(proxy, method, args);
            }
            try {
                return tile.base.getClass().getMethod(name, method.getParameterTypes()).invoke(tile.base, args);
            } catch (NoSuchMethodException e) {
                // the plain state does not know it either
            } catch (InvocationTargetException e) {
                throw e.getCause();
            }
            throw Unsupported.hit(declaring.getSimpleName() + "#" + name);
        }
    }
}

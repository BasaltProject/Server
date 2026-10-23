package org.basaltmc.bukkit.worldgen;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import org.basaltmc.bukkit.BasaltWorld;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.block.BlockState;
import org.bukkit.generator.LimitedRegion;

/** The world seen through a few chunks around one: reaching outside it is refused, as it is for any populator. */
public final class BasaltLimitedRegion {
    private BasaltLimitedRegion() {}

    public static LimitedRegion of(BasaltWorld world, int centerX, int centerZ, int buffer) {
        return (LimitedRegion) Proxy.newProxyInstance(LimitedRegion.class.getClassLoader(),
            new Class<?>[] {LimitedRegion.class}, (proxy, method, args) -> {
                String name = method.getName();
                switch (name) {
                    case "getBuffer":
                        return buffer;
                    case "getWorld":
                        return world;
                    case "getCenterChunkX":
                        return centerX;
                    case "getCenterChunkZ":
                        return centerZ;
                    case "isInRegion":
                        return args.length == 1 ? inside(centerX, centerZ, buffer, (Location) args[0])
                            : inside(centerX, centerZ, buffer, (int) args[0], (int) args[2]);
                    case "getTileEntities": {
                        List<BlockState> out = new ArrayList<>();
                        for (int x = centerX - 1; x <= centerX + 1; x++) {
                            for (int z = centerZ - 1; z <= centerZ + 1; z++) {
                                for (BlockState state : world.getChunkAt(x, z, false).getTileEntities()) {
                                    out.add(state);
                                }
                            }
                        }
                        return out;
                    }
                    case "setBlockState":
                        if (args.length == 4) {
                            check(centerX, centerZ, buffer, (int) args[0], (int) args[2]);
                            ((BlockState) args[3]).update(true, false);
                            return null;
                        }
                        break;
                    case "scheduleBlockUpdate":
                    case "scheduleFluidUpdate":
                        if (args.length == 3) {
                            check(centerX, centerZ, buffer, (int) args[0], (int) args[2]);
                            org.basaltmc.bukkit.WorldNatives.scheduleTick(world.getName(), (int) args[0], (int) args[1],
                                (int) args[2], 0, name.equals("scheduleFluidUpdate"));
                            return null;
                        }
                        break;
                    case "equals":
                        return proxy == args[0];
                    case "hashCode":
                        return System.identityHashCode(proxy);
                    case "toString":
                        return "LimitedRegion{" + centerX + "," + centerZ + "}";
                    default:
                        break;
                }
                for (Object argument : args == null ? new Object[0] : args) {
                    if (argument instanceof Location at) {
                        check(centerX, centerZ, buffer, at.getBlockX(), at.getBlockZ());
                    }
                }
                if (args != null && args.length >= 3 && args[0] instanceof Integer x && args[2] instanceof Integer z) {
                    check(centerX, centerZ, buffer, x, z);
                }
                try {
                    Method same = World.class.getMethod(name, method.getParameterTypes());
                    return same.invoke(world, args);
                } catch (NoSuchMethodException e) {
                    if (method.isDefault()) {
                        return java.lang.reflect.InvocationHandler.invokeDefault(proxy, method, args);
                    }
                    throw new UnsupportedOperationException("LimitedRegion#" + name);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                }
            });
    }

    private static boolean inside(int centerX, int centerZ, int buffer, Location at) {
        return inside(centerX, centerZ, buffer, at.getBlockX(), at.getBlockZ());
    }

    private static boolean inside(int centerX, int centerZ, int buffer, int x, int z) {
        int lowX = (centerX << 4) - buffer;
        int lowZ = (centerZ << 4) - buffer;
        return x >= lowX && x < lowX + 16 + 2 * buffer && z >= lowZ && z < lowZ + 16 + 2 * buffer;
    }

    private static void check(int centerX, int centerZ, int buffer, int x, int z) {
        if (!inside(centerX, centerZ, buffer, x, z)) {
            throw new IllegalArgumentException("Position (" + x + ", " + z + ") is outside of the region of chunk "
                + centerX + ", " + centerZ);
        }
    }
}

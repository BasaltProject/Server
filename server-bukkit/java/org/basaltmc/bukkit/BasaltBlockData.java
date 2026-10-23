package org.basaltmc.bukkit;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import org.bukkit.Axis;
import org.bukkit.Material;
import org.bukkit.block.BlockFace;
import org.bukkit.block.PistonMoveReaction;
import org.bukkit.block.data.BlockData;

public final class BasaltBlockData implements InvocationHandler {
    private static final BlockFace[] kRotations = {BlockFace.SOUTH, BlockFace.SOUTH_SOUTH_WEST, BlockFace.SOUTH_WEST,
        BlockFace.WEST_SOUTH_WEST, BlockFace.WEST, BlockFace.WEST_NORTH_WEST, BlockFace.NORTH_WEST,
        BlockFace.NORTH_NORTH_WEST, BlockFace.NORTH, BlockFace.NORTH_NORTH_EAST, BlockFace.NORTH_EAST,
        BlockFace.EAST_NORTH_EAST, BlockFace.EAST, BlockFace.EAST_SOUTH_EAST, BlockFace.SOUTH_EAST,
        BlockFace.SOUTH_SOUTH_EAST};
    private static final BlockFace[] kFaces = {BlockFace.NORTH, BlockFace.EAST, BlockFace.SOUTH, BlockFace.WEST,
        BlockFace.UP, BlockFace.DOWN};

    private int state;

    private BasaltBlockData(int state) {
        this.state = state;
    }

    public static BlockData of(int state) {
        return org.bukkit.craftbukkit.block.data.CraftBlockData.wrap(proxy(state));
    }

    private static BlockData proxy(int state) {
        Material material = Registries.materialOfState(state);
        Class<?> specific = material.data;
        Class<?> face = BlockData.class.isAssignableFrom(specific) ? specific : BlockData.class;
        java.lang.reflect.Constructor<?> maker = kMakers.get(face);
        try {
            if (maker == null) {
                Object first = Proxy.newProxyInstance(BlockData.class.getClassLoader(), new Class<?>[] {face},
                    new BasaltBlockData(state));
                kMakers.put(face, first.getClass().getConstructor(InvocationHandler.class));
                return (BlockData) first;
            }
            return (BlockData) maker.newInstance(new BasaltBlockData(state));
        } catch (ReflectiveOperationException e) {
            return (BlockData) Proxy.newProxyInstance(BlockData.class.getClassLoader(), new Class<?>[] {face},
                new BasaltBlockData(state));
        }
    }

    private static final java.util.concurrent.ConcurrentHashMap<Class<?>, java.lang.reflect.Constructor<?>> kMakers =
        new java.util.concurrent.ConcurrentHashMap<>();

    public static int stateOf(BlockData data) {
        BasaltBlockData handler = handler(data);
        if (handler != null) {
            return handler.state;
        }
        throw new IllegalArgumentException("Not block data made by this server: " + data);
    }

    private static BasaltBlockData handler(BlockData data) {
        if (data instanceof org.bukkit.craftbukkit.block.data.CraftBlockData wrapped) {
            return handler(wrapped.inner0());
        }
        if (data != null && Proxy.isProxyClass(data.getClass())
            && Proxy.getInvocationHandler(data) instanceof BasaltBlockData own) {
            return own;
        }
        return null;
    }

    public static BlockData parse(String text) {
        String trimmed = text.trim();
        if (trimmed.indexOf(':') < 0) {
            trimmed = "minecraft:" + trimmed;
        }
        int state = WorldNatives.stateId(trimmed);
        return state < 0 ? null : of(state);
    }

    private int block() {
        return Registries.blockOfState(state);
    }

    private Registries.Properties properties() {
        return Registries.propertiesOf(block());
    }

    private String get(String name) {
        Registries.Properties properties = properties();
        int index = properties.indexOf(name);
        if (index < 0) {
            return null;
        }
        int offset = state - Registries.firstState(block());
        String[] values = properties.values()[index];
        return values[(offset / properties.strides()[index]) % values.length];
    }

    private void set(String name, String value) {
        Registries.Properties properties = properties();
        int index = properties.indexOf(name);
        if (index < 0) {
            throw new IllegalArgumentException(Registries.blockName(block()) + " has no property " + name);
        }
        String[] values = properties.values()[index];
        int wanted = -1;
        for (int i = 0; i < values.length; i++) {
            if (values[i].equals(value)) {
                wanted = i;
            }
        }
        if (wanted < 0) {
            throw new IllegalArgumentException(name + " cannot be " + value);
        }
        int offset = state - Registries.firstState(block());
        int current = (offset / properties.strides()[index]) % values.length;
        state += (wanted - current) * properties.strides()[index];
    }

    private static String snake(String camel) {
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < camel.length(); i++) {
            char c = camel.charAt(i);
            if (Character.isUpperCase(c)) {
                if (i > 0) {
                    out.append('_');
                }
                out.append(Character.toLowerCase(c));
            } else {
                out.append(c);
            }
        }
        return out.toString();
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        String name = method.getName();
        int count = args == null ? 0 : args.length;
        switch (name) {
            case "equals": {
                BasaltBlockData other = args[0] instanceof BlockData data ? handler(data) : null;
                return other != null && other.state == state;
            }
            case "hashCode":
                return state;
            case "toString":
                return "BasaltBlockData{" + WorldNatives.stateText(state) + "}";
            case "getMaterial":
                return Registries.materialOfState(state);
            case "getPlacementMaterial":
                return Registries.materialOfState(state);
            case "getAsString":
                return WorldNatives.stateText(state);
            case "clone":
                return of(state);
            case "merge":
                return merge((BlockData) args[0]);
            case "matches":
                return matches((BlockData) args[0]);
            case "copyTo":
                if (stateOf((BlockData) args[0]) != state) {
                    copyInto((BlockData) args[0]);
                }
                return null;
            case "getCollisionShape":
                return org.basaltmc.bukkit.misc.Shapes.shape(state);
            case "getLightEmission":
                return (WorldNatives.stateFlags(state) >> 9) & 15;
            case "isOccluding":
                return (WorldNatives.stateFlags(state) & 2) != 0;
            case "isReplaceable":
                return (WorldNatives.stateFlags(state) & 8) != 0;
            case "isRandomlyTicked":
                return (WorldNatives.stateFlags(state) & 128) != 0;
            case "requiresCorrectToolForDrops": {
                double[] facts = WorldNatives.blockHardness(Registries.blockName(block()));
                return facts != null && facts[2] != 0.0;
            }
            case "isPreferredTool":
                return false;
            case "getDestroySpeed": {
                double[] facts = WorldNatives.blockHardness(Registries.blockName(block()));
                return facts == null ? 0.0f : (float) facts[0];
            }
            case "getPistonMoveReaction":
                return PistonMoveReaction.MOVE;
            case "isSupported":
                return true;
            case "isFaceSturdy":
                return (WorldNatives.stateFlags(state) & 4) != 0;
            case "createBlockState":
                return new BasaltBlockState(null, 0, 0, 0, of(state));
            case "rotate":
            case "mirror":
                throw Unsupported.hit("BlockData#" + name);
            case "getFaces":
                return faces(true);
            case "getAllowedFaces":
                return faces(false);
            case "hasFace":
                return "true".equals(get(((BlockFace) args[0]).name().toLowerCase(Locale.ROOT)));
            case "setFace":
                set(((BlockFace) args[0]).name().toLowerCase(Locale.ROOT), String.valueOf(args[1]));
                return null;
            case "getAxes": {
                Set<Axis> axes = EnumSet.noneOf(Axis.class);
                Registries.Properties properties = properties();
                int index = properties.indexOf("axis");
                if (index >= 0) {
                    for (String value : properties.values()[index]) {
                        axes.add(Axis.valueOf(value.toUpperCase(Locale.ROOT)));
                    }
                }
                return axes;
            }
            case "getRotation": {
                String rotation = get("rotation");
                return rotation == null ? BlockFace.SOUTH : kRotations[Integer.parseInt(rotation)];
            }
            case "setRotation": {
                int found = java.util.Arrays.asList(kRotations).indexOf((BlockFace) args[0]);
                if (found < 0) {
                    throw new IllegalArgumentException("Invalid face " + args[0]);
                }
                set("rotation", Integer.toString(found));
                return null;
            }
            default:
                break;
        }
        if (count == 0 && (name.startsWith("getMaximum") || name.startsWith("getMinimum"))) {
            String[] values = valuesOf(snake(name.substring(10)));
            int best = name.startsWith("getMaximum") ? Integer.MIN_VALUE : Integer.MAX_VALUE;
            for (String value : values) {
                int number = Integer.parseInt(value);
                best = name.startsWith("getMaximum") ? Math.max(best, number) : Math.min(best, number);
            }
            return best;
        }
        String property = null;
        if (name.startsWith("get") && count == 0) {
            property = snake(name.substring(3));
        } else if (name.startsWith("is") && count == 0) {
            property = snake(name.substring(2));
        } else if (name.startsWith("set") && count == 1) {
            property = snake(name.substring(3));
        }
        if (property != null) {
            if (count == 1) {
                set(property, toText(args[0], property));
                return null;
            }
            String value = get(property);
            if (value == null) {
                if (method.isDefault()) {
                    return InvocationHandler.invokeDefault(proxy, method, args);
                }
                throw Unsupported.hit("BlockData#" + name + " on " + Registries.blockName(block()));
            }
            return fromText(value, method.getReturnType(), property);
        }
        if (method.isDefault()) {
            return InvocationHandler.invokeDefault(proxy, method, args);
        }
        throw Unsupported.hit("BlockData#" + name);
    }

    private String[] valuesOf(String property) {
        Registries.Properties properties = properties();
        int index = properties.indexOf(property);
        return index < 0 ? new String[0] : properties.values()[index];
    }

    private Set<BlockFace> faces(boolean onlySet) {
        Set<BlockFace> out = EnumSet.noneOf(BlockFace.class);
        Registries.Properties properties = properties();
        int facing = properties.indexOf("facing");
        if (facing >= 0) {
            if (onlySet) {
                out.add(BlockFace.valueOf(get("facing").toUpperCase(Locale.ROOT)));
            } else {
                for (String value : properties.values()[facing]) {
                    out.add(BlockFace.valueOf(value.toUpperCase(Locale.ROOT)));
                }
            }
            return out;
        }
        for (BlockFace face : kFaces) {
            String value = get(face.name().toLowerCase(Locale.ROOT));
            if (value != null && (!onlySet || value.equals("true"))) {
                out.add(face);
            }
        }
        return out;
    }

    private static Object fromText(String value, Class<?> type, String property) {
        if (type == boolean.class || type == Boolean.class) {
            return Boolean.parseBoolean(value);
        }
        if (type == int.class || type == Integer.class) {
            return Integer.parseInt(value);
        }
        if (type == String.class) {
            return value;
        }
        if (type.isEnum()) {
            String upper = value.toUpperCase(Locale.ROOT);
            if (property.equals("half") && upper.equals("UPPER")) {
                upper = "TOP";
            } else if (property.equals("half") && upper.equals("LOWER")) {
                upper = "BOTTOM";
            }
            for (Object constant : type.getEnumConstants()) {
                if (((Enum<?>) constant).name().equals(upper)) {
                    return constant;
                }
            }
            throw new IllegalArgumentException(type.getSimpleName() + " has no " + upper);
        }
        throw Unsupported.hit("BlockData property " + property + " as " + type.getName());
    }

    private String toText(Object value, String property) {
        if (value instanceof Boolean || value instanceof Integer) {
            return value.toString();
        }
        if (value instanceof Enum<?> constant) {
            String lower = constant.name().toLowerCase(Locale.ROOT);
            if (property.equals("half")) {
                String[] options = valuesOf("half");
                for (String option : options) {
                    if (option.equals("upper") && lower.equals("top")) {
                        return "upper";
                    }
                    if (option.equals("lower") && lower.equals("bottom")) {
                        return "lower";
                    }
                }
            }
            return lower;
        }
        return String.valueOf(value);
    }

    private BlockData merge(BlockData other) {
        BasaltBlockData data = handler(other);
        if (data == null || data.block() != block()) {
            throw new IllegalArgumentException("Cannot merge " + other.getMaterial() + " into "
                + Registries.materialOfState(state));
        }
        return of(data.state);
    }

    private boolean matches(BlockData other) {
        BasaltBlockData data = handler(other);
        return data != null && data.state == state;
    }

    private void copyInto(BlockData target) {
        BasaltBlockData data = handler(target);
        if (data != null && data.block() == block()) {
            data.state = state;
        }
    }

    public static List<String[]> propertiesOfState(int state) {
        List<String[]> out = new ArrayList<>();
        int block = Registries.blockOfState(state);
        Registries.Properties properties = Registries.propertiesOf(block);
        int offset = state - Registries.firstState(block);
        for (int i = 0; i < properties.names().length; i++) {
            String[] values = properties.values()[i];
            out.add(new String[] {properties.names()[i], values[(offset / properties.strides()[i]) % values.length]});
        }
        return out;
    }
}

package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;

/** Adapter over the native brain of one entity: Java values go in and out as the tagged strings the native side parses. */
public final class NativeBrain {
    private static final char kSeparator = '|';
    private final LivingEntity owner;

    public NativeBrain(LivingEntity owner) {
        this.owner = owner;
    }

    public boolean bound() {
        return owner.getId() >= 0 && owner.level() != null;
    }

    public Object call(String operation, Object... arguments) {
        if (!bound()) {
            return null;
        }
        return EntityNatives.call(owner.level().getWorld().getName(), owner.getId(), operation, arguments);
    }

    public boolean test(String operation, Object... arguments) {
        return call(operation, arguments) instanceof Boolean value && value;
    }

    public String[] strings(String operation, Object... arguments) {
        return call(operation, arguments) instanceof String[] value ? value : new String[0];
    }

    /** Scalars cross as themselves; everything else as a tagged list of strings, or null when only presence matters. */
    public Object encode(Object value) {
        if (value instanceof Boolean || value instanceof String) {
            return value;
        }
        if (value instanceof Integer || value instanceof Long || value instanceof Short || value instanceof Byte) {
            return ((Number) value).longValue();
        }
        if (value instanceof Float || value instanceof Double) {
            return ((Number) value).doubleValue();
        }
        if (value instanceof Collection<?> items) {
            List<String> out = new ArrayList<>();
            out.add("list");
            for (Object item : items) {
                String element = element(item);
                if (element == null) {
                    return marker();
                }
                out.add(element);
            }
            return out.toArray(new String[0]);
        }
        String single = element(value);
        return single == null ? marker() : single.split("\\|", -1);
    }

    private static String[] marker() {
        return new String[] {"marker"};
    }

    private static String number(double value) {
        return Double.toString(value);
    }

    private static String element(Object value) {
        if (value instanceof Boolean flag) {
            return "b|" + (flag ? 1 : 0);
        }
        if (value instanceof Integer || value instanceof Long || value instanceof Short || value instanceof Byte) {
            return "i|" + ((Number) value).longValue();
        }
        if (value instanceof Float || value instanceof Double) {
            return "r|" + number(((Number) value).doubleValue());
        }
        if (value instanceof String text) {
            return "t|" + text;
        }
        if (value instanceof Entity entity) {
            return "entity|" + entity.getId();
        }
        if (value instanceof BlockPos pos) {
            return "position||" + pos.getX() + "|" + pos.getY() + "|" + pos.getZ();
        }
        if (value instanceof GlobalPos global) {
            BlockPos pos = global.pos();
            return "position|" + global.dimension().identifier() + "|" + pos.getX() + "|" + pos.getY() + "|" + pos.getZ();
        }
        if (value instanceof Vec3 at) {
            return "position||" + number(at.x) + "|" + number(at.y) + "|" + number(at.z);
        }
        if (value instanceof WalkTarget walk) {
            Vec3 at = walk.getTarget().currentPosition();
            return "walk|" + number(at.x) + "|" + number(at.y) + "|" + number(at.z) + "|-1|" + number(walk.getSpeedModifier())
                + "|" + walk.getCloseEnoughDist();
        }
        return null;
    }

    /** The comparison key of an encoded value, so a payload the native side no longer agrees with is dropped. */
    public static String fingerprint(Object encoded) {
        if (encoded instanceof String[] parts) {
            return String.join("\u0001", parts);
        }
        return String.valueOf(encoded);
    }

    public Object decode(Object wire) {
        if (wire instanceof Boolean || wire instanceof Double || wire instanceof String) {
            return wire;
        }
        if (wire instanceof Long whole) {
            return whole >= Integer.MIN_VALUE && whole <= Integer.MAX_VALUE ? (Object) whole.intValue() : whole;
        }
        if (!(wire instanceof String[] parts) || parts.length == 0) {
            return null;
        }
        if (parts[0].equals("list")) {
            List<Object> out = new ArrayList<>();
            for (int index = 1; index < parts.length; index++) {
                Object item = decodeElement(parts[index]);
                if (item != null) {
                    out.add(item);
                }
            }
            return out;
        }
        return decodeElement(String.join(String.valueOf(kSeparator), parts));
    }

    private Object decodeElement(String text) {
        String[] parts = text.split("\\|", -1);
        switch (parts[0]) {
            case "b":
                return parts.length > 1 && parts[1].equals("1");
            case "i":
                return Integer.parseInt(parts[1]);
            case "r":
                return Double.parseDouble(parts[1]);
            case "t":
                return text.substring(2);
            case "entity": {
                Entity found = owner.level().getEntity(Integer.parseInt(parts[1]));
                return found;
            }
            case "position": {
                BlockPos pos = BlockPos.containing(Double.parseDouble(parts[2]), Double.parseDouble(parts[3]),
                    Double.parseDouble(parts[4]));
                if (parts[1].isEmpty()) {
                    return pos;
                }
                ResourceKey<Level> dimension = ResourceKey.create(Registries.DIMENSION, Identifier.parse(parts[1]));
                return GlobalPos.of(dimension, pos);
            }
            case "walk": {
                Vec3 at = new Vec3(Double.parseDouble(parts[1]), Double.parseDouble(parts[2]), Double.parseDouble(parts[3]));
                return new WalkTarget(at, Float.parseFloat(parts[5]), Integer.parseInt(parts[6]));
            }
            default:
                return null;
        }
    }
}

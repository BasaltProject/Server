package org.basaltmc.bukkit;

import java.lang.reflect.Method;
import java.lang.reflect.ParameterizedType;
import java.util.EnumSet;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;
import org.bukkit.Color;
import org.bukkit.NamespacedKey;
import org.bukkit.Note;
import org.bukkit.Registry;
import org.bukkit.Sound;
import org.bukkit.SoundGroup;
import org.bukkit.block.Block;
import org.bukkit.block.BlockFace;
import org.bukkit.block.PistonMoveReaction;
import org.bukkit.block.structure.Mirror;
import org.bukkit.block.structure.StructureRotation;
import org.bukkit.enchantments.Enchantment;
import org.bukkit.inventory.ItemStack;

/** The BlockData methods whose property is not just the method name, and the ones that need the server. */
final class BlockDataSpecials {
    static final Object kNone = new Object();

    private BlockDataSpecials() {}

    /** The sound type of a block, answering both the Bukkit and the Paper interface. */
    static final class Sounds implements SoundGroup, com.destroystokyo.paper.block.BlockSoundGroup {
        private final String[] facts;

        Sounds(String[] facts) {
            this.facts = facts;
        }

        private static Sound sound(String id) {
            return Registry.SOUNDS.get(NamespacedKey.fromString(id));
        }

        @Override
        public float getVolume() {
            return Float.parseFloat(facts[0]);
        }

        @Override
        public float getPitch() {
            return Float.parseFloat(facts[1]);
        }

        @Override
        public Sound getBreakSound() {
            return sound(facts[2]);
        }

        @Override
        public Sound getStepSound() {
            return sound(facts[3]);
        }

        @Override
        public Sound getPlaceSound() {
            return sound(facts[4]);
        }

        @Override
        public Sound getHitSound() {
            return sound(facts[5]);
        }

        @Override
        public Sound getFallSound() {
            return sound(facts[6]);
        }
    }

    static Sounds soundsOf(int state) {
        String[] facts = WorldNatives.blockSounds(state);
        if (facts == null) {
            throw new IllegalArgumentException("No such block state " + state);
        }
        return new Sounds(facts);
    }

    private static String itemId(ItemStack tool) {
        return tool == null || tool.getType().isAir() ? "" : tool.getType().getKey().toString();
    }

    private static boolean needsTool(int state) {
        double[] facts = WorldNatives.blockHardness(Registries.blockName(Registries.blockOfState(state)));
        return facts != null && facts[2] != 0.0;
    }

    static boolean preferredTool(int state, ItemStack tool) {
        if (!needsTool(state)) {
            return true;
        }
        double[] facts = WorldEnvNatives.toolFacts(itemId(tool), state);
        return facts != null && facts[0] != 0.0;
    }

    static float destroySpeed(int state, ItemStack tool, boolean enchants) {
        double[] facts = WorldEnvNatives.toolFacts(itemId(tool), state);
        float speed = facts == null ? 1.0F : (float) facts[1];
        if (speed > 1.0F && enchants && tool != null) {
            int level = tool.getEnchantmentLevel(Enchantment.EFFICIENCY);
            if (level > 0) {
                speed += level * level + 1;
            }
        }
        return speed;
    }

    static PistonMoveReaction reaction(int state) {
        double[] info = WorldNatives.blockInfo(state);
        PistonMoveReaction[] all = {PistonMoveReaction.MOVE, PistonMoveReaction.BREAK, PistonMoveReaction.BLOCK,
            PistonMoveReaction.IGNORE, PistonMoveReaction.PUSH_ONLY};
        return info == null ? PistonMoveReaction.MOVE : all[(int) Math.max(0, Math.min(4, info[3]))];
    }

    private static Object enumOf(Class<?> type, String text) {
        String upper = text.toUpperCase(Locale.ROOT);
        for (Object constant : type.getEnumConstants()) {
            if (((Enum<?>) constant).name().equals(upper)) {
                return constant;
            }
        }
        throw new IllegalArgumentException(type.getSimpleName() + " has no " + upper);
    }

    private static String lower(Object value) {
        return ((Enum<?>) value).name().toLowerCase(Locale.ROOT);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Set<Object> allowed(BasaltBlockData data, String property, Method method) {
        Class<?> element = (Class<?>) ((ParameterizedType) method.getGenericReturnType()).getActualTypeArguments()[0];
        Set<Object> out = new java.util.LinkedHashSet<>();
        for (String value : data.valuesOf(property)) {
            out.add(enumOf(element, value));
        }
        return out;
    }

    private static Set<Integer> flagged(BasaltBlockData data, String pattern, int count) {
        Set<Integer> out = new TreeSet<>();
        for (int i = 0; i < count; i++) {
            if ("true".equals(data.get(String.format(pattern, i)))) {
                out.add(i);
            }
        }
        return out;
    }

    private static boolean hasProperty(BasaltBlockData data, String property) {
        return data.valuesOf(property).length > 0;
    }

    static Object call(BasaltBlockData data, int state, Method method, String name, Object[] args) {
        int count = args == null ? 0 : args.length;
        switch (name) {
            case "getSoundGroup":
                return count == 0 ? soundsOf(state) : kNone;
            case "getMapColor": {
                double[] info = count == 0 ? WorldNatives.blockInfo(state) : null;
                return info == null ? kNone : Color.fromRGB((int) info[4] & 0xFFFFFF);
            }
            case "getPistonMoveReaction":
                return count == 0 ? reaction(state) : kNone;
            case "isPreferredTool":
                return count == 1 ? preferredTool(state, (ItemStack) args[0]) : kNone;
            case "getDestroySpeed":
                return count == 2 ? destroySpeed(state, (ItemStack) args[0], (Boolean) args[1]) : kNone;
            case "isSupported": {
                if (count != 1) {
                    return kNone;
                }
                Blocks.flushAll();
                if (args[0] instanceof Block block) {
                    return WorldEnvNatives.survives(block.getWorld().getName(), block.getX(), block.getY(),
                        block.getZ(), state);
                }
                org.bukkit.Location at = (org.bukkit.Location) args[0];
                return WorldEnvNatives.survives(at.getWorld().getName(), at.getBlockX(), at.getBlockY(),
                    at.getBlockZ(), state);
            }
            case "rotate":
                data.setState(WorldEnvNatives.transformState(state, ((StructureRotation) args[0]).ordinal(), 0));
                return null;
            case "mirror":
                data.setState(WorldEnvNatives.transformState(state, 0, ((Mirror) args[0]).ordinal()));
                return null;
            case "getAttachedFace":
                return count == 0 && hasProperty(data, "face")
                    ? enumOf(method.getReturnType(), data.get("face")) : kNone;
            case "setAttachedFace":
                if (count == 1 && hasProperty(data, "face")) {
                    data.set("face", lower(args[0]));
                    return null;
                }
                return kNone;
            case "getNote":
                return count == 0 && hasProperty(data, "note") ? new Note(Integer.parseInt(data.get("note"))) : kNone;
            case "setNote":
                if (count == 1 && hasProperty(data, "note")) {
                    data.set("note", Integer.toString(((Note) args[0]).getId()));
                    return null;
                }
                return kNone;
            case "getFace":
            case "getHeight":
                if (count == 1 && args[0] instanceof BlockFace face && hasProperty(data, lower(face))) {
                    return enumOf(method.getReturnType(), data.get(lower(face)));
                }
                return kNone;
            case "setFace":
            case "setHeight":
                if (count == 2 && args[0] instanceof BlockFace face && args[1] instanceof Enum<?> value
                    && hasProperty(data, lower(face))) {
                    data.set(lower(face), lower(value));
                    return null;
                }
                return kNone;
            case "hasBottle":
                return count == 1 ? "true".equals(data.get("has_bottle_" + args[0])) : kNone;
            case "setBottle":
                if (count == 2) {
                    data.set("has_bottle_" + args[0], String.valueOf(args[1]));
                    return null;
                }
                return kNone;
            case "getBottles":
                return count == 0 ? flagged(data, "has_bottle_%d", 3) : kNone;
            case "getMaximumBottles":
                return count == 0 ? 3 : kNone;
            case "isSlotOccupied":
                return count == 1 ? "true".equals(data.get("slot_" + args[0] + "_occupied")) : kNone;
            case "setSlotOccupied":
                if (count == 2) {
                    data.set("slot_" + args[0] + "_occupied", String.valueOf(args[1]));
                    return null;
                }
                return kNone;
            case "getOccupiedSlots":
                return count == 0 ? flagged(data, "slot_%d_occupied", 6) : kNone;
            case "getMaximumOccupiedSlots":
                return count == 0 ? 6 : kNone;
            case "getVerticalDirections":
                return count == 0 ? allowed(data, "vertical_direction", method) : kNone;
            case "getShapes":
                return count == 0 ? allowed(data, "shape", method) : kNone;
            default:
                return kNone;
        }
    }
}

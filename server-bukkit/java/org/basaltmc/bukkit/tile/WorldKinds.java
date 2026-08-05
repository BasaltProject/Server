package org.basaltmc.bukkit.tile;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.WorldEnvNatives;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.block.Beacon;
import org.bukkit.block.Bell;
import org.bukkit.block.BlockFace;
import org.bukkit.block.BlockState;
import org.bukkit.block.Campfire;
import org.bukkit.block.CommandBlock;
import org.bukkit.block.EndGateway;
import org.bukkit.block.EnderChest;
import org.bukkit.block.Structure;
import org.bukkit.block.structure.Mirror;
import org.bukkit.block.structure.StructureRotation;
import org.bukkit.block.structure.UsageMode;
import org.bukkit.entity.Entity;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Player;
import org.bukkit.inventory.ItemStack;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.util.BlockVector;

final class WorldKinds {
    private WorldKinds() {}

    static final class BeaconKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Beacon.class;
        }

        private static PotionEffect effect(Tile tile, String key, boolean primary) {
            String id = tile.text(key, null);
            NamespacedKey name = id == null ? null : NamespacedKey.fromString(id);
            PotionEffectType type = name == null ? null : Registry.EFFECT.get(name);
            if (type == null) {
                return null;
            }
            int levels = tile.integer("Levels", 0);
            boolean doubled = primary && levels >= 4 && id.equals(tile.text("secondary_effect", null));
            return new PotionEffect(type, (9 + levels * 2) * 20, doubled ? 1 : 0, true, true);
        }

        private static void set(Tile tile, String key, Object type) {
            tile.put(key, type == null ? null : ((PotionEffectType) type).getKey().toString());
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getTier":
                    return tile.integer("Levels", 0);
                case "getPrimaryEffect":
                    return effect(tile, "primary_effect", true);
                case "getSecondaryEffect":
                    return effect(tile, "secondary_effect", false);
                case "setPrimaryEffect":
                    set(tile, "primary_effect", args[0]);
                    return null;
                case "setSecondaryEffect":
                    set(tile, "secondary_effect", args[0]);
                    return null;
                case "getEffectRange":
                    {
                    double custom = org.basaltmc.bukkit.item.Nbt.real(tile.data, "BeaconRange", -1.0);
                    return custom >= 0.0 ? custom : tile.integer("Levels", 0) * 10.0 + 10.0;
                }
                case "setEffectRange":
                    tile.put("BeaconRange", ((Number) args[0]).doubleValue());
                    return null;
                case "resetEffectRange":
                    tile.put("BeaconRange", null);
                    return null;
                case "getEntitiesInRange": {
                    List<LivingEntity> out = new ArrayList<>();
                    double range = tile.integer("Levels", 0) * 10.0 + 10.0;
                    if (tile.integer("Levels", 0) <= 0) {
                        return out;
                    }
                    Location center = tile.location().add(0.5, 0.5, 0.5);
                    for (Player player : tile.world.getPlayers()) {
                        Location at = player.getLocation();
                        if (Math.abs(at.getX() - center.getX()) <= range && Math.abs(at.getZ() - center.getZ()) <= range) {
                            out.add(player);
                        }
                    }
                    return out;
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class CommandBlockKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return CommandBlock.class;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getCommand":
                    return tile.text("Command", "");
                case "setCommand":
                    tile.put("Command", args[0] == null ? "" : args[0]);
                    return null;
                case "getName": {
                    Object raw = tile.data.get("CustomName");
                    return raw == null ? "@" : Texts.legacyFromNbt(raw);
                }
                case "setName":
                    tile.put("CustomName", args[0] == null ? null : Texts.legacyToNbt((String) args[0]));
                    return null;
                case "name": {
                    if (((Object[]) (args == null ? new Object[0] : args)).length == 1) {
                        tile.put("CustomName", args[0] == null ? null : Texts.toNbt((Component) args[0]));
                        return null;
                    }
                    Object raw = tile.data.get("CustomName");
                    return raw == null ? Component.text("@") : Texts.fromNbt(raw);
                }
                case "lastOutput": {
                    if (args != null && args.length == 1) {
                        tile.put("LastOutput", args[0] == null ? null : Texts.toNbt((Component) args[0]));
                        return null;
                    }
                    Object raw = tile.data.get("LastOutput");
                    return raw == null ? null : Texts.fromNbt(raw);
                }
                case "getSuccessCount":
                    return tile.integer("SuccessCount", 0);
                case "setSuccessCount":
                    tile.put("SuccessCount", (Integer) args[0]);
                    return null;
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class StructureKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Structure.class;
        }

        private static BlockVector vector(Tile tile, String x, String y, String z) {
            return new BlockVector(tile.integer(x, 0), tile.integer(y, 0), tile.integer(z, 0));
        }

        private static void put(Tile tile, String x, String y, String z, BlockVector at) {
            tile.put(x, at.getBlockX());
            tile.put(y, at.getBlockY());
            tile.put(z, at.getBlockZ());
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getStructureName":
                    return tile.text("name", "");
                case "setStructureName":
                    tile.put("name", args[0]);
                    return null;
                case "getAuthor":
                    return tile.text("author", "");
                case "setAuthor":
                    tile.put("author", args[0] instanceof LivingEntity entity ? entity.getName() : args[0]);
                    return null;
                case "getRelativePosition":
                    return vector(tile, "posX", "posY", "posZ");
                case "setRelativePosition":
                    put(tile, "posX", "posY", "posZ", (BlockVector) args[0]);
                    return null;
                case "getStructureSize":
                    return vector(tile, "sizeX", "sizeY", "sizeZ");
                case "setStructureSize":
                    put(tile, "sizeX", "sizeY", "sizeZ", (BlockVector) args[0]);
                    return null;
                case "getMirror":
                    return Mirror.valueOf(tile.text("mirror", "NONE"));
                case "setMirror":
                    tile.put("mirror", ((Mirror) args[0]).name());
                    return null;
                case "getRotation":
                    return StructureRotation.valueOf(tile.text("rotation", "NONE"));
                case "setRotation":
                    tile.put("rotation", ((StructureRotation) args[0]).name());
                    return null;
                case "getUsageMode":
                    return UsageMode.valueOf(tile.text("mode", "DATA"));
                case "setUsageMode":
                    tile.put("mode", ((UsageMode) args[0]).name());
                    return null;
                case "isIgnoreEntities":
                    return tile.flag("ignoreEntities", true);
                case "setIgnoreEntities":
                    tile.put("ignoreEntities", Nbt.bool((Boolean) args[0]));
                    return null;
                case "isShowAir":
                    return tile.flag("showair", false);
                case "setShowAir":
                    tile.put("showair", Nbt.bool((Boolean) args[0]));
                    return null;
                case "isBoundingBoxVisible":
                    return tile.flag("showboundingbox", true);
                case "setBoundingBoxVisible":
                    tile.put("showboundingbox", Nbt.bool((Boolean) args[0]));
                    return null;
                case "getIntegrity":
                    return (float) Nbt.real(tile.data, "integrity", 1.0);
                case "setIntegrity":
                    tile.put("integrity", (Float) args[0]);
                    return null;
                case "getSeed":
                    return Nbt.longValue(tile.data, "seed", 0L);
                case "setSeed":
                    tile.put("seed", (Long) args[0]);
                    return null;
                case "getMetadata":
                    return tile.text("metadata", "");
                case "setMetadata":
                    tile.put("metadata", args[0]);
                    return null;
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class GatewayKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return EndGateway.class;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getExitLocation": {
                    Object raw = tile.data.get("exit_portal");
                    if (!(raw instanceof int[] at) || at.length < 3) {
                        return null;
                    }
                    return new Location(tile.world, at[0], at[1], at[2]);
                }
                case "setExitLocation": {
                    Location at = (Location) args[0];
                    tile.put("exit_portal", at == null ? null
                        : new int[] {at.getBlockX(), at.getBlockY(), at.getBlockZ()});
                    return null;
                }
                case "isExactTeleport":
                    return tile.flag("ExactTeleport", false);
                case "setExactTeleport":
                    tile.put("ExactTeleport", Nbt.bool((Boolean) args[0]));
                    return null;
                case "getAge":
                    return Nbt.longValue(tile.data, "Age", 0L);
                case "setAge":
                    tile.put("Age", (Long) args[0]);
                    return null;
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class CampfireKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Campfire.class;
        }

        private static List<Object> items(Tile tile) {
            List<Object> list = Nbt.list(tile.data, "Items");
            return list == null ? new ArrayList<>() : list;
        }

        private static int[] array(Tile tile, String key) {
            Object raw = tile.data.get(key);
            int[] out = new int[4];
            if (raw instanceof int[] ints) {
                System.arraycopy(ints, 0, out, 0, Math.min(4, ints.length));
            }
            return out;
        }

        private static void check(int slot) {
            if (slot < 0 || slot >= 4) {
                throw new IndexOutOfBoundsException("Slot " + slot + " is outside of 0 to 3");
            }
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getSize":
                    return 4;
                case "getItem": {
                    int slot = (Integer) args[0];
                    check(slot);
                    for (Object value : items(tile)) {
                        Map<String, Object> entry = Nbt.map(value);
                        if (entry != null && Nbt.integer(entry, "Slot", -1) == slot) {
                            return Convert.stack(entry);
                        }
                    }
                    return null;
                }
                case "setItem": {
                    int slot = (Integer) args[0];
                    check(slot);
                    List<Object> list = items(tile);
                    list.removeIf(value -> Nbt.map(value) != null && Nbt.integer(Nbt.map(value), "Slot", -1) == slot);
                    ItemStack stack = (ItemStack) args[1];
                    if (stack != null && !stack.getType().isAir()) {
                        Map<String, Object> entry = Convert.stack(stack);
                        entry.put("Slot", (byte) slot);
                        list.add(entry);
                    }
                    tile.put("Items", list);
                    return null;
                }
                case "getCookTime": {
                    check((Integer) args[0]);
                    return array(tile, "CookingTimes")[(Integer) args[0]];
                }
                case "setCookTime": {
                    check((Integer) args[0]);
                    int[] values = array(tile, "CookingTimes");
                    values[(Integer) args[0]] = (Integer) args[1];
                    tile.put("CookingTimes", values);
                    return null;
                }
                case "getCookTimeTotal": {
                    check((Integer) args[0]);
                    return array(tile, "CookingTotalTimes")[(Integer) args[0]];
                }
                case "setCookTimeTotal": {
                    check((Integer) args[0]);
                    int[] values = array(tile, "CookingTotalTimes");
                    values[(Integer) args[0]] = (Integer) args[1];
                    tile.put("CookingTotalTimes", values);
                    return null;
                }
                case "isCookingDisabled": {
                    check((Integer) args[0]);
                    return array(tile, "Paper.StopCooking")[(Integer) args[0]] != 0;
                }
                case "startCooking", "stopCooking": {
                    int[] values = array(tile, "Paper.StopCooking");
                    int wanted = name.equals("stopCooking") ? 1 : 0;
                    boolean changed = false;
                    if (args == null || args.length == 0) {
                        for (int slot = 0; slot < values.length; slot++) {
                            changed |= values[slot] != wanted;
                            values[slot] = wanted;
                        }
                        tile.put("Paper.StopCooking", values);
                        return null;
                    }
                    check((Integer) args[0]);
                    changed = values[(Integer) args[0]] != wanted;
                    values[(Integer) args[0]] = wanted;
                    tile.put("Paper.StopCooking", values);
                    return changed;
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class BellKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Bell.class;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "ring": {
                    BlockFace face = args == null || args.length == 0 ? null
                        : args[args.length - 1] instanceof BlockFace given ? given : null;
                    if (face == null && args != null && args.length == 2) {
                        face = (BlockFace) args[1];
                    }
                    int direction = face == null ? 0 : switch (face) {
                        case DOWN -> 0;
                        case UP -> 1;
                        case NORTH -> 2;
                        case SOUTH -> 3;
                        case WEST -> 4;
                        default -> 5;
                    };
                    WorldEnvNatives.blockEvent(tile.world.getName(), tile.x, tile.y, tile.z, 1, direction);
                    WorldEnvNatives.sound(tile.world.getName(), "minecraft:block.bell.use", "blocks", tile.x + 0.5,
                        tile.y + 0.5, tile.z + 0.5, 2.0F, 1.0F, 0L);
                    return true;
                }
                case "isShaking", "getShakingTicks", "isResonating", "getResonatingTicks": {
                    Map<String, Object> live = tile.data;
                    return switch (name) {
                        case "isShaking" -> Nbt.flag(live, "Basalt.BellShaking", false);
                        case "getShakingTicks" -> Nbt.flag(live, "Basalt.BellShaking", false)
                            ? Nbt.integer(live, "Basalt.BellTicks", 0) : 0;
                        case "isResonating" -> Nbt.flag(live, "Basalt.BellResonating", false);
                        default -> Nbt.flag(live, "Basalt.BellResonating", false)
                            ? Nbt.integer(live, "Basalt.BellResonationTicks", 0) : 0;
                    };
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class EnderChestKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return EnderChest.class;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "isBlocked": {
                    return tile.live().getRelative(BlockFace.UP).getType().isOccluding();
                }
                default:
                    return Tile.kNone;
            }
        }
    }
}

package org.basaltmc.bukkit.tile;

import io.papermc.paper.datacomponent.item.ResolvableProfile;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Map;
import org.basaltmc.bukkit.item.MetaSkull;
import org.bukkit.Material;
import org.bukkit.SkullType;
import org.bukkit.block.BlockFace;
import org.bukkit.block.BlockState;
import org.bukkit.block.Skull;
import org.bukkit.block.data.Directional;
import org.bukkit.block.data.Rotatable;

final class SkullKind implements TileKind {
    @Override
    public Class<? extends BlockState> face() {
        return Skull.class;
    }

    private static SkullType typeOf(String block) {
        String name = block.replace("_wall", "");
        return switch (name) {
            case "skeleton_skull" -> SkullType.SKELETON;
            case "wither_skeleton_skull" -> SkullType.WITHER;
            case "zombie_head" -> SkullType.ZOMBIE;
            case "creeper_head" -> SkullType.CREEPER;
            case "dragon_head" -> SkullType.DRAGON;
            case "piglin_head" -> SkullType.PIGLIN;
            default -> SkullType.PLAYER;
        };
    }

    private static Material materialOf(SkullType type, boolean wall) {
        String name = switch (type) {
            case SKELETON -> "skeleton_skull";
            case WITHER -> "wither_skeleton_skull";
            case ZOMBIE -> "zombie_head";
            case CREEPER -> "creeper_head";
            case DRAGON -> "dragon_head";
            case PIGLIN -> "piglin_head";
            default -> "player_head";
        };
        return Material.matchMaterial(wall ? name.replace("_skull", "_wall_skull").replace("_head", "_wall_head") : name);
    }

    @Override
    public Object call(Tile tile, Method method, String name, Object[] args) throws Throwable {
        switch (name) {
            case "getSkullType":
                return typeOf(tile.block);
            case "setSkullType": {
                Material target = materialOf((SkullType) args[0], tile.block.contains("_wall_"));
                String text = tile.blockData().getAsString();
                int open = text.indexOf('[');
                tile.base.setBlockData(
                    org.bukkit.Bukkit.createBlockData(target.getKey() + (open < 0 ? "" : text.substring(open))));
                return null;
            }
            case "getRotation": {
                var data = tile.blockData();
                if (data instanceof Rotatable rotatable) {
                    return rotatable.getRotation();
                }
                return data instanceof Directional directional ? directional.getFacing() : BlockFace.NORTH;
            }
            case "setRotation": {
                var data = tile.blockData();
                if (data instanceof Rotatable rotatable) {
                    rotatable.setRotation((BlockFace) args[0]);
                } else if (data instanceof Directional directional) {
                    directional.setFacing((BlockFace) args[0]);
                }
                tile.base.setBlockData(data);
                return null;
            }
            case "getProfile": {
                var profile = ((MetaSkull) meta(tile)).getPlayerProfile();
                return profile == null ? null : ResolvableProfile.resolvableProfile(profile);
            }
            case "setProfile": {
                ResolvableProfile profile = (ResolvableProfile) args[0];
                if (profile == null) {
                    ((MetaSkull) meta(tile)).setPlayerProfile(null);
                    return null;
                }
                var made = org.bukkit.Bukkit.createProfile(profile.uuid(), profile.name());
                made.setProperties(profile.properties());
                ((MetaSkull) meta(tile)).setPlayerProfile(made);
                return null;
            }
            case "hasOwner", "getOwner", "setOwner", "getOwningPlayer", "setOwningPlayer", "setPlayerProfile",
                "getPlayerProfile", "getOwnerProfile", "setOwnerProfile", "getNoteBlockSound", "setNoteBlockSound":
                try {
                    MetaSkull meta = (MetaSkull) meta(tile);
                    return meta.getClass().getMethod(name, method.getParameterTypes()).invoke(meta, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                }
            default:
                return Tile.kNone;
        }
    }

    private static Object meta(Tile tile) {
        return new MetaSkull(Material.PLAYER_HEAD, new ComponentView(tile.data, Map.of()));
    }
}

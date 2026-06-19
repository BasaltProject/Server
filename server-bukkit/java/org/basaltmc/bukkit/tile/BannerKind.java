package org.basaltmc.bukkit.tile;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Map;
import org.basaltmc.bukkit.item.MetaBanner;
import org.bukkit.DyeColor;
import org.bukkit.Material;
import org.bukkit.block.Banner;
import org.bukkit.block.Bed;
import org.bukkit.block.BlockState;

/** Banners keep their patterns in the data and their colour in the block; beds keep only the block's colour. */
final class BannerKind implements TileKind {
    private final boolean bed;

    BannerKind(boolean bed) {
        this.bed = bed;
    }

    @Override
    public Class<? extends BlockState> face() {
        return bed ? Bed.class : Banner.class;
    }

    private static DyeColor colorOf(String block, String suffix) {
        String name = block.replace("_wall", "");
        return DyeColor.valueOf(name.substring(0, name.length() - suffix.length() - 1).toUpperCase(java.util.Locale.ROOT));
    }

    private static void recolor(Tile tile, DyeColor color, String suffix) {
        String wall = tile.block.contains("_wall_") ? "_wall" : "";
        Material target = Material.matchMaterial(color.name().toLowerCase(java.util.Locale.ROOT) + wall + "_" + suffix);
        org.bukkit.block.data.BlockData old = tile.blockData();
        String text = old.getAsString();
        int open = text.indexOf('[');
        tile.base.setBlockData(org.bukkit.Bukkit.createBlockData(target.getKey() + (open < 0 ? "" : text.substring(open))));
    }

    @Override
    public Object call(Tile tile, Method method, String name, Object[] args) throws Throwable {
        if (bed) {
            return switch (name) {
                case "getColor" -> colorOf(tile.block, "bed");
                case "setColor" -> {
                    recolor(tile, (DyeColor) args[0], "bed");
                    yield null;
                }
                default -> Tile.kNone;
            };
        }
        switch (name) {
            case "getBaseColor":
                return colorOf(tile.block, "banner");
            case "setBaseColor":
                recolor(tile, (DyeColor) args[0], "banner");
                return null;
            case "getPatterns", "setPatterns", "addPattern", "getPattern", "removePattern", "setPattern",
                "numberOfPatterns":
                try {
                    MetaBanner meta = new MetaBanner(Material.WHITE_BANNER,
                        new ComponentView(tile.data, Map.of("minecraft:banner_patterns", "patterns")));
                    return meta.getClass().getMethod(name, method.getParameterTypes()).invoke(meta, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                }
            default:
                return Tile.kNone;
        }
    }
}

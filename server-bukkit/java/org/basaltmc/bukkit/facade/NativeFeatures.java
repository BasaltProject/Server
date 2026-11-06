package org.basaltmc.bukkit.facade;

import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.Feature;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import org.basaltmc.bukkit.RestNatives;

/** Placed features of the native world generator, addressed by id. */
public final class NativeFeatures {
    private NativeFeatures() {}

    public static PlacedFeature placed(Identifier id) {
        return new PlacedFeature(HolderRefs.<Feature>of(Feature.class, id), List.of());
    }

    public static boolean place(WorldGenLevel level, Identifier id, BlockPos origin, long seed) {
        org.basaltmc.bukkit.Blocks.flushAll();
        boolean direct = level instanceof net.minecraft.server.level.ServerLevel;
        return apply(level, RestNatives.placeFeature(level.getLevel().getWorld().getName(), id.toString(), origin.getX(),
            origin.getY(), origin.getZ(), seed, direct), direct);
    }

    /** A named tree of the native generator placed at the origin. */
    public static boolean placeTree(WorldGenLevel level, String tree, BlockPos origin) {
        org.basaltmc.bukkit.Blocks.flushAll();
        boolean direct = level instanceof net.minecraft.server.level.ServerLevel;
        return apply(level, RestNatives.placeTree(level.getLevel().getWorld().getName(), tree, origin.getX(), origin.getY(),
            origin.getZ(), direct), direct);
    }

    private static boolean apply(WorldGenLevel level, int[] placed, boolean direct) {
        if (placed == null) {
            return false;
        }
        if (!direct) {
            for (int at = 0; at + 4 < placed.length; at += 5) {
                level.setBlock(new BlockPos(placed[at], placed[at + 1], placed[at + 2]),
                    net.minecraft.world.level.block.Block.stateById(placed[at + 4]), 3);
            }
        }
        return placed.length > 0;
    }

    /** The log family a trunk block belongs to, such as {@code oak} for {@code oak_log}, or null for other blocks. */
    public static String woodOf(net.minecraft.world.level.block.state.BlockState trunk) {
        String path = net.minecraft.core.registries.BuiltInRegistries.BLOCK.getKey(trunk.getBlock()).getPath();
        return path.endsWith("_log") ? path.substring(0, path.length() - 4) : null;
    }

    public static java.util.List<Identifier> ids() {
        java.util.List<Identifier> out = new java.util.ArrayList<>();
        for (String id : RestNatives.featureIds()) {
            out.add(Identifier.parse(id));
        }
        return out;
    }
}

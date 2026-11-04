package org.basaltmc.bukkit.facade;

import java.util.HashSet;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.server.level.ServerLevel;
import org.basaltmc.bukkit.WorldEnvNatives;

/** Answers which generated structures have a piece at a block, from the native structure starts. */
public final class StructureLookup {
    private StructureLookup() {}

    public static Set<String> piecesAt(ServerLevel level, BlockPos pos) {
        Set<String> found = new HashSet<>();
        int chunkX = pos.getX() >> 4;
        int chunkZ = pos.getZ() >> 4;
        for (int dx = -8; dx <= 8; dx++) {
            for (int dz = -8; dz <= 8; dz++) {
                String[] rows = WorldEnvNatives.structureStarts(level.getWorld().getName(), chunkX + dx, chunkZ + dz);
                for (String row : rows == null ? new String[0] : rows) {
                    String[] parts = row.split("\\|", -1);
                    if (parts.length >= 5 && anyContains(parts[4], pos)) {
                        found.add(parts[0]);
                    }
                }
            }
        }
        return found;
    }

    private static boolean anyContains(String boxes, BlockPos pos) {
        for (String box : boxes.split(";")) {
            String[] n = box.split(",");
            if (n.length == 6 && pos.getX() >= Integer.parseInt(n[0]) && pos.getX() <= Integer.parseInt(n[3])
                && pos.getY() >= Integer.parseInt(n[1]) && pos.getY() <= Integer.parseInt(n[4])
                && pos.getZ() >= Integer.parseInt(n[2]) && pos.getZ() <= Integer.parseInt(n[5])) {
                return true;
            }
        }
        return false;
    }

    public static boolean inAny(ServerLevel level, BlockPos pos, HolderSet<?> structures) {
        Set<String> here = piecesAt(level, pos);
        if (here.isEmpty()) {
            return false;
        }
        for (net.minecraft.core.Holder<?> holder : structures) {
            if (holder.unwrapKey().isPresent() && here.contains(holder.unwrapKey().get().identifier().toString())) {
                return true;
            }
        }
        return false;
    }
}

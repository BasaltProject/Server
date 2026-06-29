package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.util.BoundingBox;
import org.bukkit.util.VoxelShape;

/** The collision shapes of block states, read once from the server's tables. */
public final class Shapes {
    private static final Map<Integer, double[]> kBoxes = new ConcurrentHashMap<>();
    private static final double kUnits = 32.0;

    private Shapes() {}

    /** Six numbers per box (min corner, then max corner) in blocks, relative to the block. */
    public static double[] boxes(int state) {
        return org.basaltmc.bukkit.facade.Memo.get(kBoxes, state, id -> {
            int[] raw = MiscNatives.shapeBoxes(id);
            double[] out = new double[raw == null ? 0 : raw.length];
            for (int i = 0; i < out.length; i++) {
                out[i] = raw[i] / kUnits;
            }
            return out;
        });
    }

    public static VoxelShape shape(int state) {
        double[] boxes = boxes(state);
        List<BoundingBox> list = new ArrayList<>(boxes.length / 6);
        for (int i = 0; i + 5 < boxes.length; i += 6) {
            list.add(new BoundingBox(boxes[i], boxes[i + 1], boxes[i + 2], boxes[i + 3], boxes[i + 4], boxes[i + 5]));
        }
        return new BasaltVoxelShape(list);
    }

    /** The box around the whole shape of a block at a position, or an empty box on it when there is none. */
    public static BoundingBox bounds(int state, int x, int y, int z) {
        double[] boxes = boxes(state);
        if (boxes.length == 0) {
            return new BoundingBox(x, y, z, x, y, z);
        }
        double minX = Double.MAX_VALUE, minY = Double.MAX_VALUE, minZ = Double.MAX_VALUE;
        double maxX = -Double.MAX_VALUE, maxY = -Double.MAX_VALUE, maxZ = -Double.MAX_VALUE;
        for (int i = 0; i + 5 < boxes.length; i += 6) {
            minX = Math.min(minX, boxes[i]);
            minY = Math.min(minY, boxes[i + 1]);
            minZ = Math.min(minZ, boxes[i + 2]);
            maxX = Math.max(maxX, boxes[i + 3]);
            maxY = Math.max(maxY, boxes[i + 4]);
            maxZ = Math.max(maxZ, boxes[i + 5]);
        }
        return new BoundingBox(x + minX, y + minY, z + minZ, x + maxX, y + maxY, z + maxZ);
    }

    private record BasaltVoxelShape(List<BoundingBox> list) implements VoxelShape {
        @Override
        public java.util.Collection<BoundingBox> getBoundingBoxes() {
            List<BoundingBox> copy = new ArrayList<>(list.size());
            for (BoundingBox box : list) {
                copy.add(box.clone());
            }
            return copy;
        }

        @Override
        public boolean overlaps(BoundingBox other) {
            for (BoundingBox box : list) {
                if (box.overlaps(other)) {
                    return true;
                }
            }
            return false;
        }
    }
}

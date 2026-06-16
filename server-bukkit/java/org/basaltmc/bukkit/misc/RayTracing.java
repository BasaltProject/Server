package org.basaltmc.bukkit.misc;

import java.util.function.Predicate;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.FluidCollisionMode;
import org.bukkit.Material;
import org.bukkit.World;
import org.bukkit.block.Block;
import org.bukkit.block.BlockFace;
import org.bukkit.entity.Entity;
import org.bukkit.util.BoundingBox;
import org.bukkit.util.RayTraceResult;
import org.bukkit.util.Vector;

/** Rays through the collision shapes of blocks and the boxes of entities. */
public final class RayTracing {
    private RayTracing() {}

    private static final class Entry {
        double distance = Double.MAX_VALUE;
        BlockFace face;
    }

    /** Where a ray enters one box, as a distance along it, or a negative number when it misses. */
    private static double enter(double[] origin, double[] step, double[] box, double max, Entry into) {
        double near = 0.0;
        double far = max;
        BlockFace face = null;
        BlockFace[][] faces = {{BlockFace.WEST, BlockFace.EAST}, {BlockFace.DOWN, BlockFace.UP},
            {BlockFace.NORTH, BlockFace.SOUTH}};
        for (int axis = 0; axis < 3; axis++) {
            double low = box[axis];
            double high = box[axis + 3];
            if (Math.abs(step[axis]) < 1.0e-12) {
                if (origin[axis] < low || origin[axis] > high) {
                    return -1.0;
                }
                continue;
            }
            double t1 = (low - origin[axis]) / step[axis];
            double t2 = (high - origin[axis]) / step[axis];
            BlockFace entering = faces[axis][step[axis] > 0 ? 0 : 1];
            if (t1 > t2) {
                double swap = t1;
                t1 = t2;
                t2 = swap;
            }
            if (t1 > near) {
                near = t1;
                face = entering;
            }
            far = Math.min(far, t2);
            if (near > far) {
                return -1.0;
            }
        }
        into.face = face;
        return near;
    }

    private static boolean passable(Block block) {
        return block.getType().isAir() || Shapes.boxes(WorldNatives.state(block.getWorld().getName(), block.getX(),
            block.getY(), block.getZ(), false)).length == 0;
    }

    public static RayTraceResult block(Block block, Vector start, Vector direction, double maxDistance,
            FluidCollisionMode fluids, boolean ignorePassable) {
        Vector dir = direction.clone().normalize();
        double[] origin = {start.getX(), start.getY(), start.getZ()};
        double[] step = {dir.getX(), dir.getY(), dir.getZ()};
        int state = WorldNatives.state(block.getWorld().getName(), block.getX(), block.getY(), block.getZ(), false);
        double[] boxes = Shapes.boxes(state);
        Material type = block.getType();
        boolean fluid = type == Material.WATER || type == Material.LAVA;
        boolean full = fluid ? fluids == FluidCollisionMode.ALWAYS
            || fluids == FluidCollisionMode.SOURCE_ONLY && block.getBlockData() instanceof org.bukkit.block.data.Levelled level
                && level.getLevel() == 0
            : boxes.length == 0 && !ignorePassable && !type.isAir();
        if (full) {
            boxes = new double[] {0, 0, 0, 1, 1, 1};
        }
        Entry best = new Entry();
        for (int i = 0; i + 5 < boxes.length; i += 6) {
            double[] box = {block.getX() + boxes[i], block.getY() + boxes[i + 1], block.getZ() + boxes[i + 2],
                block.getX() + boxes[i + 3], block.getY() + boxes[i + 4], block.getZ() + boxes[i + 5]};
            Entry entry = new Entry();
            double t = enter(origin, step, box, maxDistance, entry);
            if (t >= 0 && t < best.distance) {
                best.distance = t;
                best.face = entry.face;
            }
        }
        if (best.distance == Double.MAX_VALUE) {
            return null;
        }
        Vector hit = start.clone().add(dir.multiply(best.distance));
        return new RayTraceResult(hit, block, best.face);
    }

    public static RayTraceResult blocks(World world, Vector start, Vector direction, double maxDistance,
            FluidCollisionMode fluids, boolean ignorePassable, Predicate<? super Block> canCollide) {
        if (direction.lengthSquared() == 0.0) {
            throw new IllegalArgumentException("Direction's magnitude (" + direction.length() + ") must be greater than 0");
        }
        Vector dir = direction.clone().normalize();
        double max = Math.min(maxDistance, 1024.0);
        int x = (int) Math.floor(start.getX());
        int y = (int) Math.floor(start.getY());
        int z = (int) Math.floor(start.getZ());
        int stepX = dir.getX() > 0 ? 1 : -1;
        int stepY = dir.getY() > 0 ? 1 : -1;
        int stepZ = dir.getZ() > 0 ? 1 : -1;
        double deltaX = dir.getX() == 0 ? Double.MAX_VALUE : Math.abs(1.0 / dir.getX());
        double deltaY = dir.getY() == 0 ? Double.MAX_VALUE : Math.abs(1.0 / dir.getY());
        double deltaZ = dir.getZ() == 0 ? Double.MAX_VALUE : Math.abs(1.0 / dir.getZ());
        double nextX = dir.getX() == 0 ? Double.MAX_VALUE
            : (dir.getX() > 0 ? x + 1 - start.getX() : start.getX() - x) * deltaX;
        double nextY = dir.getY() == 0 ? Double.MAX_VALUE
            : (dir.getY() > 0 ? y + 1 - start.getY() : start.getY() - y) * deltaY;
        double nextZ = dir.getZ() == 0 ? Double.MAX_VALUE
            : (dir.getZ() > 0 ? z + 1 - start.getZ() : start.getZ() - z) * deltaZ;
        double travelled = 0.0;
        while (true) {
            Block block = world.getBlockAt(x, y, z);
            if (canCollide == null || canCollide.test(block)) {
                RayTraceResult hit = block(block, start, dir, max, fluids, ignorePassable);
                if (hit != null) {
                    return hit;
                }
            }
            if (nextX <= nextY && nextX <= nextZ) {
                travelled = nextX;
                nextX += deltaX;
                x += stepX;
            } else if (nextY <= nextZ) {
                travelled = nextY;
                nextY += deltaY;
                y += stepY;
            } else {
                travelled = nextZ;
                nextZ += deltaZ;
                z += stepZ;
            }
            if (travelled > max) {
                return null;
            }
        }
    }

    public static RayTraceResult entities(World world, Vector start, Vector direction, double maxDistance,
            double raySize, Predicate<? super Entity> filter) {
        if (direction.lengthSquared() == 0.0) {
            throw new IllegalArgumentException("Direction's magnitude (" + direction.length() + ") must be greater than 0");
        }
        Vector dir = direction.clone().normalize();
        Vector end = start.clone().add(dir.clone().multiply(maxDistance));
        BoundingBox region = BoundingBox.of(start, end).expand(raySize);
        RayTraceResult nearest = null;
        double nearestDistance = Double.MAX_VALUE;
        for (Entity entity : world.getNearbyEntities(region, filter)) {
            BoundingBox box = entity.getBoundingBox().expand(raySize);
            RayTraceResult hit;
            if (box.contains(start)) {
                hit = new RayTraceResult(start.clone(), (BlockFace) null);
            } else {
                hit = box.rayTrace(start, dir, maxDistance);
            }
            if (hit == null) {
                continue;
            }
            double distance = start.distanceSquared(hit.getHitPosition());
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = new RayTraceResult(hit.getHitPosition(), entity, hit.getHitBlockFace());
            }
        }
        return nearest;
    }

    public static RayTraceResult both(World world, Vector start, Vector direction, double maxDistance,
            FluidCollisionMode fluids, boolean ignorePassable, double raySize, Predicate<? super Entity> filter,
            Predicate<? super Block> canCollide) {
        RayTraceResult blockHit = blocks(world, start, direction, maxDistance, fluids, ignorePassable, canCollide);
        double reach = blockHit == null ? maxDistance : start.distance(blockHit.getHitPosition());
        RayTraceResult entityHit = entities(world, start, direction, reach, raySize, filter);
        if (entityHit == null) {
            return blockHit;
        }
        if (blockHit == null) {
            return entityHit;
        }
        return start.distanceSquared(entityHit.getHitPosition()) <= start.distanceSquared(blockHit.getHitPosition())
            ? entityHit : blockHit;
    }
}

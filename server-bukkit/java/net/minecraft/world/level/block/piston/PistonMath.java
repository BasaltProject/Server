// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.piston;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.AABB;
public class PistonMath {
    public static AABB getMovementArea(AABB aabb, Direction direction, double amount) {
        double delta = amount * direction.getAxisDirection().getStep();
        double min = Math.min(delta, 0.0);
        double max = Math.max(delta, 0.0);
        return switch (direction) {
            case WEST -> new AABB(aabb.minX + min, aabb.minY, aabb.minZ, aabb.minX + max, aabb.maxY, aabb.maxZ);
            case EAST -> new AABB(aabb.maxX + min, aabb.minY, aabb.minZ, aabb.maxX + max, aabb.maxY, aabb.maxZ);
            case DOWN -> new AABB(aabb.minX, aabb.minY + min, aabb.minZ, aabb.maxX, aabb.minY + max, aabb.maxZ);
            case UP -> new AABB(aabb.minX, aabb.maxY + min, aabb.minZ, aabb.maxX, aabb.maxY + max, aabb.maxZ);
            case NORTH -> new AABB(aabb.minX, aabb.minY, aabb.minZ + min, aabb.maxX, aabb.maxY, aabb.minZ + max);
            case SOUTH -> new AABB(aabb.minX, aabb.minY, aabb.maxZ + min, aabb.maxX, aabb.maxY, aabb.maxZ + max);
        };
    }
}

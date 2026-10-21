package org.basaltmc.bukkit;

import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.World;
import org.bukkit.block.BlockFace;
import org.basaltmc.bukkit.gen.BlockBase;

/** A position in a world; its material is read from the server when asked. */
public final class BasaltBlock extends BlockBase {
    private final BasaltWorld world;
    private final int x;
    private final int y;
    private final int z;

    BasaltBlock(BasaltWorld world, int x, int y, int z) {
        this.world = world;
        this.x = x;
        this.y = y;
        this.z = z;
    }

    @Override
    public World getWorld() {
        return world;
    }

    @Override
    public int getX() {
        return x;
    }

    @Override
    public int getY() {
        return y;
    }

    @Override
    public int getZ() {
        return z;
    }

    @Override
    public Location getLocation() {
        return new Location(world, x, y, z);
    }

    @Override
    public Material getType() {
        return world.materialAt(x, y, z);
    }

    @Override
    public void setType(Material type) {
        Natives.setBlockAt(world.getName(), x, y, z, type.getKey().toString());
    }

    @Override
    public void setType(Material type, boolean applyPhysics) {
        setType(type);
    }

    @Override
    public org.bukkit.block.Block getRelative(BlockFace face) {
        return getRelative(face.getModX(), face.getModY(), face.getModZ());
    }

    @Override
    public org.bukkit.block.Block getRelative(int dx, int dy, int dz) {
        return new BasaltBlock(world, x + dx, y + dy, z + dz);
    }

    @Override
    public boolean isEmpty() {
        return getType().isAir();
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltBlock block && block.world.equals(world) && block.x == x && block.y == y
            && block.z == z;
    }

    @Override
    public int hashCode() {
        return (x * 31 + y) * 31 + z;
    }
}

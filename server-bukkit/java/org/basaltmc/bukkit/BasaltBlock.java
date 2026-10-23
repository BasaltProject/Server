package org.basaltmc.bukkit;

import java.util.Collection;
import java.util.List;
import org.bukkit.Chunk;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.World;
import org.bukkit.block.Biome;
import org.bukkit.block.BlockFace;
import org.bukkit.block.BlockState;
import org.bukkit.block.PistonMoveReaction;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.Entity;
import org.bukkit.inventory.ItemStack;
import org.bukkit.metadata.MetadataValue;
import org.bukkit.plugin.Plugin;
import org.basaltmc.bukkit.gen.BlockBase;

/** A position in a world; every question is answered by the server when asked, so the object never goes stale. */
public final class BasaltBlock extends BlockBase {
    private static final MetadataHolder metadata = new MetadataHolder();

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

    int state() {
        int state = world.stateAt(x, y, z);
        return state < 0 ? 0 : state;
    }

    private int flags() {
        return WorldNatives.stateFlags(state());
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
    public Location getLocation(Location into) {
        if (into != null) {
            into.setWorld(world);
            into.setX(x);
            into.setY(y);
            into.setZ(z);
        }
        return into;
    }

    @Override
    public Chunk getChunk() {
        return world.getChunkAt(x >> 4, z >> 4);
    }

    @Override
    public Material getType() {
        return Registries.materialOfState(state());
    }

    @Override
    public BlockData getBlockData() {
        return BasaltBlockData.of(state());
    }

    @Override
    public byte getData() {
        return 0;
    }

    @Override
    public void setType(Material type) {
        setType(type, true);
    }

    @Override
    public void setType(Material type, boolean applyPhysics) {
        int state = Registries.defaultStateOf(type);
        if (state < 0) {
            throw new IllegalArgumentException(type + " is not a block");
        }
        world.putState(x, y, z, state);
    }

    @Override
    public void setBlockData(BlockData data) {
        setBlockData(data, true);
    }

    @Override
    public void setBlockData(BlockData data, boolean applyPhysics) {
        world.putState(x, y, z, BasaltBlockData.stateOf(data));
    }

    @Override
    public BlockState getState() {
        return org.basaltmc.bukkit.inv.ContainerStates.of(world.getName(), x, y, z,
            new BasaltBlockState(world, x, y, z, getBlockData()));
    }

    @Override
    public BlockState getState(boolean useSnapshot) {
        return getState();
    }

    @Override
    public org.bukkit.block.Block getRelative(BlockFace face) {
        return getRelative(face.getModX(), face.getModY(), face.getModZ());
    }

    @Override
    public org.bukkit.block.Block getRelative(BlockFace face, int distance) {
        return getRelative(face.getModX() * distance, face.getModY() * distance, face.getModZ() * distance);
    }

    @Override
    public org.bukkit.block.Block getRelative(int dx, int dy, int dz) {
        return new BasaltBlock(world, x + dx, y + dy, z + dz);
    }

    @Override
    public BlockFace getFace(org.bukkit.block.Block block) {
        for (BlockFace face : BlockFace.values()) {
            if (x + face.getModX() == block.getX() && y + face.getModY() == block.getY()
                && z + face.getModZ() == block.getZ() && block.getWorld().equals(world)) {
                return face;
            }
        }
        return null;
    }

    @Override
    public byte getLightLevel() {
        return (byte) Math.max(getLightFromBlocks(), getLightFromSky());
    }

    @Override
    public byte getLightFromSky() {
        return 0;
    }

    @Override
    public byte getLightFromBlocks() {
        return (byte) ((flags() >> 9) & 15);
    }

    @Override
    public Biome getBiome() {
        return world.getBiome(x, y, z);
    }

    @Override
    public Biome getComputedBiome() {
        return world.getBiome(x, y, z);
    }

    @Override
    public void setBiome(Biome biome) {
        world.setBiome(x, y, z, biome);
    }

    @Override
    public boolean isEmpty() {
        return (flags() & 256) != 0;
    }

    @Override
    public boolean isLiquid() {
        return (flags() & 16) != 0 && (flags() & 32) == 0;
    }

    @Override
    public boolean isBuildable() {
        return (flags() & 8) == 0 && (flags() & 256) == 0;
    }

    @Override
    public boolean isBurnable() {
        return getType().isBurnable();
    }

    @Override
    public boolean isReplaceable() {
        return (flags() & 8) != 0;
    }

    @Override
    public boolean isSolid() {
        return (flags() & 1) != 0;
    }

    @Override
    public boolean isCollidable() {
        return (flags() & 32) != 0;
    }

    @Override
    public boolean isPassable() {
        return (flags() & 32) == 0;
    }

    @Override
    public boolean isSuffocating() {
        return (flags() & 2) != 0;
    }

    @Override
    public boolean isBlockPowered() {
        return false;
    }

    @Override
    public boolean isBlockIndirectlyPowered() {
        return false;
    }

    @Override
    public PistonMoveReaction getPistonMoveReaction() {
        return PistonMoveReaction.MOVE;
    }

    @Override
    public double getTemperature() {
        return 0.8;
    }

    @Override
    public double getHumidity() {
        return 0.4;
    }

    @Override
    public boolean breakNaturally() {
        return breakNaturally(null, false, false, false);
    }

    @Override
    public boolean breakNaturally(ItemStack tool) {
        return breakNaturally(tool, false, false, false);
    }

    @Override
    public boolean breakNaturally(boolean triggerEffect, boolean dropExperience) {
        return breakNaturally(null, triggerEffect, dropExperience, false);
    }

    @Override
    public boolean breakNaturally(ItemStack tool, boolean triggerEffect, boolean dropExperience) {
        return breakNaturally(tool, triggerEffect, dropExperience, false);
    }

    @Override
    public boolean breakNaturally(ItemStack tool, boolean triggerEffect, boolean dropExperience, boolean forceEffect) {
        if (isEmpty()) {
            return false;
        }
        world.putState(x, y, z, Registries.defaultStateOf(Material.AIR));
        return true;
    }

    @Override
    public Collection<ItemStack> getDrops() {
        return List.of();
    }

    @Override
    public Collection<ItemStack> getDrops(ItemStack tool) {
        return List.of();
    }

    @Override
    public Collection<ItemStack> getDrops(ItemStack tool, Entity entity) {
        return List.of();
    }

    @Override
    public boolean isPreferredTool(ItemStack tool) {
        return false;
    }

    @Override
    public boolean isValidTool(ItemStack tool) {
        return true;
    }

    @Override
    public boolean canPlace(BlockData data) {
        return true;
    }

    @Override
    public String getTranslationKey() {
        return "block.minecraft." + getType().getKey().getKey();
    }

    @Override
    public String translationKey() {
        return getTranslationKey();
    }

    @Override
    public void setMetadata(String name, MetadataValue value) {
        metadata.set(key(name), value);
    }

    @Override
    public List<MetadataValue> getMetadata(String name) {
        return metadata.get(key(name));
    }

    @Override
    public boolean hasMetadata(String name) {
        return metadata.has(key(name));
    }

    @Override
    public void removeMetadata(String name, Plugin plugin) {
        metadata.remove(key(name), plugin);
    }

    private String key(String name) {
        return world.getName() + ":" + x + "," + y + "," + z + ":" + name;
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

    @Override
    public String toString() {
        return "BasaltBlock{world=" + world.getName() + ",x=" + x + ",y=" + y + ",z=" + z + "}";
    }

    @Override
    public org.bukkit.util.RayTraceResult rayTrace(org.bukkit.Location start, org.bukkit.util.Vector direction,
            double maxDistance, org.bukkit.FluidCollisionMode fluidCollisionMode) {
        return org.basaltmc.bukkit.misc.RayTracing.block(this, start.toVector(), direction, maxDistance,
            fluidCollisionMode, false);
    }

    @Override
    public org.bukkit.util.BoundingBox getBoundingBox() {
        return org.basaltmc.bukkit.misc.Shapes.bounds(state(), x, y, z);
    }

    @Override
    public org.bukkit.util.VoxelShape getCollisionShape() {
        return org.basaltmc.bukkit.misc.Shapes.shape(state());
    }
}

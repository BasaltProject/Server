package org.basaltmc.bukkit;

import java.util.Collection;
import java.util.List;
import org.bukkit.Chunk;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.World;
import org.bukkit.block.Block;
import org.bukkit.block.BlockState;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.Entity;
import org.bukkit.inventory.ItemStack;
import org.bukkit.material.MaterialData;
import org.bukkit.metadata.MetadataValue;
import org.bukkit.plugin.Plugin;
import org.basaltmc.bukkit.gen.BlockStateBase;

public final class BasaltBlockState extends BlockStateBase {
    private final BasaltWorld world;
    private final int x;
    private final int y;
    private final int z;
    private BlockData data;

    BasaltBlockState(BasaltWorld world, int x, int y, int z, BlockData data) {
        this.world = world;
        this.x = x;
        this.y = y;
        this.z = z;
        this.data = data;
    }

    @Override
    public Block getBlock() {
        return world.getBlockAt(x, y, z);
    }

    @Override
    public MaterialData getData() {
        return new MaterialData(data.getMaterial());
    }

    @Override
    public BlockData getBlockData() {
        return data.clone();
    }

    @Override
    public BlockState copy() {
        return new BasaltBlockState(world, x, y, z, data.clone());
    }

    @Override
    public BlockState copy(Location location) {
        return new BasaltBlockState((BasaltWorld) location.getWorld(), location.getBlockX(), location.getBlockY(),
            location.getBlockZ(), data.clone());
    }

    @Override
    public Material getType() {
        return data.getMaterial();
    }

    @Override
    public byte getLightLevel() {
        return getBlock().getLightLevel();
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
    public void setData(MaterialData materialData) {
        data = materialData.getItemType().createBlockData();
    }

    @Override
    public void setBlockData(BlockData blockData) {
        data = blockData.clone();
    }

    @Override
    public void setType(Material type) {
        data = type.createBlockData();
    }

    @Override
    public boolean update() {
        return update(false, true);
    }

    @Override
    public boolean update(boolean force) {
        return update(force, true);
    }

    @Override
    public boolean update(boolean force, boolean applyPhysics) {
        if (world == null) {
            return false;
        }
        world.getBlockAt(x, y, z).setBlockData(data, applyPhysics);
        return true;
    }

    @Override
    public byte getRawData() {
        return 0;
    }

    @Override
    public void setRawData(byte value) {}

    @Override
    public boolean isPlaced() {
        return world != null;
    }

    @Override
    public boolean isCollidable() {
        return (WorldNatives.stateFlags(BasaltBlockData.stateOf(data)) & 32) != 0;
    }

    @Override
    public Collection<ItemStack> getDrops(ItemStack tool, Entity entity) {
        return world == null ? List.of() : getBlock().getDrops(tool, entity);
    }

    @Override
    public boolean isSuffocating() {
        return (WorldNatives.stateFlags(BasaltBlockData.stateOf(data)) & 2) != 0;
    }

    private static final MetadataHolder metadata = new MetadataHolder();

    private String key(String name) {
        return world.getName() + ":" + x + "," + y + "," + z + ":" + name;
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

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltBlockState state && state.x == x && state.y == y && state.z == z
            && java.util.Objects.equals(state.world, world) && state.data.equals(data);
    }

    @Override
    public int hashCode() {
        return (x * 31 + y) * 31 + z;
    }
}

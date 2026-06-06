package org.basaltmc.bukkit.worldedit.fawe;

import com.fastasyncworldedit.core.queue.IChunkGet;
import com.sk89q.jnbt.Tag;
import com.sk89q.worldedit.world.block.BlockState;
import com.sk89q.worldedit.world.block.BlockTypesCache;
import com.sk89q.worldedit.world.registry.BlockMaterial;
import org.basaltmc.bukkit.worldedit.BasaltWorldEditAdapter;
import org.basaltmc.bukkit.worldedit.StateMapper;
import org.bukkit.World;

/** FastAsyncWorldEdit's variant of the WorldEdit adapter: ordinal state tables and whole-section chunk access. */
public final class BasaltFaweAdapter extends BasaltWorldEditAdapter implements StateMapper {
    public BasaltFaweAdapter() {
        org.bukkit.plugin.Plugin plugin = org.bukkit.Bukkit.getPluginManager().getPlugin("FastAsyncWorldEdit");
        org.bukkit.Bukkit.getScheduler().runTask(plugin, () -> com.fastasyncworldedit.core.Fawe.instance().setMainThread());
    }

    @Override
    protected StateMapper mapper() {
        return this;
    }

    @Override
    public int toState(BlockState state) {
        return StateTable.toState(state.getOrdinal());
    }

    @Override
    public BlockState toBlock(int state) {
        return BlockTypesCache.states[StateTable.toOrdinal(state)];
    }

    @Override
    protected BlockMaterial newMaterial(String id, int defaultState) {
        return new BasaltFaweBlockMaterial(id, defaultState);
    }

    @Override
    public IChunkGet get(World world, int chunkX, int chunkZ) {
        return new BasaltGetBlocks(world.getName(), chunkX, chunkZ, world.getMinHeight(), world.getMaxHeight() - 1);
    }

    @Override
    public com.fastasyncworldedit.core.extent.processor.lighting.RelighterFactory getRelighterFactory() {
        return (mode, world, queue) -> new BasaltRelighter();
    }

    public Object fromNative(Tag foreign) {
        return foreign;
    }

    public Tag toNative(Object foreign) {
        return (Tag) foreign;
    }
}

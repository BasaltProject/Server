package org.basaltmc.bukkit.worldedit;

import com.sk89q.worldedit.internal.wna.WorldNativeAccess;
import com.sk89q.worldedit.math.BlockVector3;
import com.sk89q.worldedit.world.block.BlockState;
import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.WorldNatives;
import org.enginehub.linbus.tree.LinCompoundTag;

/** Single-block edits: states are interned so the identity checks of the shared edit code work. */
public final class BasaltNativeAccess implements WorldNativeAccess<long[], Integer, BlockVector3> {
    private static final ConcurrentHashMap<Integer, Integer> kInterned = new ConcurrentHashMap<>();

    private final String world;
    private final StateMapper mapper;

    public BasaltNativeAccess(String world, StateMapper mapper) {
        this.world = world;
        this.mapper = mapper;
    }

    private static Integer intern(int state) {
        return kInterned.computeIfAbsent(state, s -> s);
    }

    @Override
    public long[] getChunk(int x, int z) {
        return new long[] {x, z};
    }

    @Override
    public Integer toNative(BlockState state) {
        return intern(mapper.toState(state));
    }

    @Override
    public Integer getBlockState(long[] chunk, BlockVector3 position) {
        return intern(WorldNatives.state(world, position.x(), position.y(), position.z(), true));
    }

    @Override
    public Integer setBlockState(long[] chunk, BlockVector3 position, Integer state) {
        return WorldNatives.setState(world, position.x(), position.y(), position.z(), state, true) ? state : null;
    }

    @Override
    public Integer getValidBlockForPosition(Integer block, BlockVector3 position) {
        return block;
    }

    @Override
    public BlockVector3 getPosition(int x, int y, int z) {
        return BlockVector3.at(x, y, z);
    }

    @Override
    public void updateLightingForBlock(BlockVector3 position) {}

    @Override
    public boolean updateTileEntity(BlockVector3 position, LinCompoundTag tag) {
        return true;
    }

    @Override
    public void notifyBlockUpdate(long[] chunk, BlockVector3 position, Integer oldState, Integer newState) {}

    @Override
    public boolean isChunkTicking(long[] chunk) {
        return true;
    }

    @Override
    public void markBlockChanged(long[] chunk, BlockVector3 position) {}

    @Override
    public void notifyNeighbors(BlockVector3 pos, Integer oldState, Integer newState) {}

    @Override
    public void updateBlock(BlockVector3 pos, Integer oldState, Integer newState) {}

    @Override
    public void updateNeighbors(BlockVector3 pos, Integer oldState, Integer newState, int recursionLimit) {}

    @Override
    public void onBlockStateChange(BlockVector3 pos, Integer oldState, Integer newState) {}

    public void flush() {}
}

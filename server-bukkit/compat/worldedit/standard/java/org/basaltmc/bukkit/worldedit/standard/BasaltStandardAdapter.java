package org.basaltmc.bukkit.worldedit.standard;

import com.sk89q.worldedit.bukkit.BukkitAdapter;
import com.sk89q.worldedit.world.block.BlockState;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicReferenceArray;
import com.sk89q.worldedit.world.registry.BlockMaterial;
import org.basaltmc.bukkit.BasaltBlockData;
import org.basaltmc.bukkit.worldedit.BasaltBlockMaterial;
import org.basaltmc.bukkit.worldedit.BasaltWorldEditAdapter;
import org.basaltmc.bukkit.worldedit.StateMapper;

/** The adapter plain WorldEdit asks the server platform for: block states travel through Bukkit block data. */
public final class BasaltStandardAdapter extends BasaltWorldEditAdapter implements StateMapper {
    private final ConcurrentHashMap<BlockState, Integer> toBasalt = new ConcurrentHashMap<>();
    private volatile AtomicReferenceArray<BlockState> toWorldEdit = new AtomicReferenceArray<>(0);

    @Override
    protected StateMapper mapper() {
        return this;
    }

    @Override
    protected BlockMaterial newMaterial(String id, int defaultState) {
        return new BasaltBlockMaterial(id, defaultState) {};
    }

    @Override
    public int toState(BlockState state) {
        Integer known = toBasalt.get(state);
        if (known != null) {
            return known;
        }
        int id = BasaltBlockData.stateOf(BukkitAdapter.adapt(state));
        toBasalt.put(state, id);
        return id;
    }

    @Override
    public BlockState toBlock(int state) {
        AtomicReferenceArray<BlockState> cache = toWorldEdit;
        if (state >= 0 && state < cache.length()) {
            BlockState found = cache.get(state);
            if (found != null) {
                return found;
            }
        } else if (state >= 0) {
            synchronized (this) {
                if (state >= toWorldEdit.length()) {
                    AtomicReferenceArray<BlockState> grown = new AtomicReferenceArray<>(Math.max(state + 1, 32768));
                    for (int i = 0; i < toWorldEdit.length(); i++) {
                        grown.set(i, toWorldEdit.get(i));
                    }
                    toWorldEdit = grown;
                }
                cache = toWorldEdit;
            }
        }
        BlockState made = BukkitAdapter.adapt(BasaltBlockData.of(Math.max(state, 0)));
        if (state >= 0 && state < cache.length()) {
            cache.set(state, made);
        }
        return made;
    }
}

package org.basaltmc.bukkit.fawe;

import com.sk89q.worldedit.registry.state.Property;
import com.sk89q.worldedit.world.block.BlockState;
import com.sk89q.worldedit.world.block.BlockType;
import com.sk89q.worldedit.world.block.BlockTypesCache;
import java.util.Arrays;
import java.util.List;
import org.basaltmc.bukkit.Registries;

/** Translates between FAWE block ordinals and Basalt block state ids, both ways, with plain array lookups. */
final class StateTable {
    private static volatile int[] ordinalToState;
    private static volatile int[] stateToOrdinal;

    private StateTable() {}

    static int toState(int ordinal) {
        return table()[ordinal];
    }

    static int toOrdinal(int state) {
        int[] reverse = reverse();
        return state >= 0 && state < reverse.length ? reverse[state] : 1;
    }

    static int stateCount() {
        return reverse().length;
    }

    private static int[] table() {
        int[] built = ordinalToState;
        if (built == null) {
            build();
            built = ordinalToState;
        }
        return built;
    }

    private static int[] reverse() {
        int[] built = stateToOrdinal;
        if (built == null) {
            build();
            built = stateToOrdinal;
        }
        return built;
    }

    private static synchronized void build() {
        if (ordinalToState != null) {
            return;
        }
        BlockState[] all = BlockTypesCache.states;
        int air = Registries.defaultState(Registries.blockIndex("minecraft:air"));
        int[] forward = new int[all.length];
        Arrays.fill(forward, air);
        int blocks = Registries.blockCount();
        int total = Registries.firstState(blocks - 1) + Registries.stateCount(blocks - 1);
        int[] backward = new int[total];
        Arrays.fill(backward, 1);
        BlockType current = null;
        int first = 0;
        int firstState = -1;
        int[] sizes = null;
        int[] strides = null;
        for (int i = 0; i < all.length; i++) {
            BlockType type = all[i].getBlockType();
            if (type != current) {
                current = type;
                first = i;
                int block = Registries.blockIndex(type.id());
                if (block < 0) {
                    firstState = -1;
                } else {
                    firstState = Registries.firstState(block);
                    Registries.Properties properties = Registries.propertiesOf(block);
                    List<? extends Property<?>> listed = type.getProperties();
                    sizes = new int[listed.size()];
                    strides = properties.strides();
                    for (int p = 0; p < sizes.length; p++) {
                        sizes[p] = listed.get(p).getValues().size();
                    }
                }
            }
            if (firstState < 0) {
                continue;
            }
            int remaining = i - first;
            int offset = 0;
            for (int p = 0; p < sizes.length; p++) {
                offset += (remaining % sizes[p]) * strides[p];
                remaining /= sizes[p];
            }
            forward[i] = firstState + offset;
        }
        for (int i = 1; i < all.length; i++) {
            int state = forward[i];
            if (state >= 0 && state < total) {
                backward[state] = i;
            }
        }
        backward[air] = 1;
        stateToOrdinal = backward;
        ordinalToState = forward;
    }
}

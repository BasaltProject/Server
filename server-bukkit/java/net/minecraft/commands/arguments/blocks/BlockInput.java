// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.blocks;

import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;
import net.minecraft.world.level.block.state.properties.Property;

public class BlockInput implements Predicate<BlockInWorld> {
    private final BlockState state;
    private final Set<Property<?>> properties;
    private final CompoundTag tag;

    public BlockInput(BlockState state, Set<Property<?>> properties, CompoundTag tag) {
        this.state = state;
        this.properties = properties;
        this.tag = tag;
    }

    public BlockState getState() {
        return state;
    }

    public Set<Property<?>> getDefinedProperties() {
        return properties;
    }

    public boolean test(BlockInWorld blockInWorld) {
        return matches(blockInWorld.getState());
    }

    public boolean test(ServerLevel level, BlockPos pos) {
        return matches(level.getBlockState(pos));
    }

    private boolean matches(BlockState other) {
        if (!other.is(state.getBlock())) {
            return false;
        }
        for (Property<?> property : properties) {
            if (other.getValue(property) != state.getValue(property)) {
                return false;
            }
        }
        return true;
    }

    public boolean place(ServerLevel level, BlockPos pos, int update) {
        BlockState placed = state;
        for (Property<?> property : level.getBlockState(pos).getProperties()) {
            if (!properties.contains(property) && placed.hasProperty(property)) {
                placed = copy(placed, level.getBlockState(pos), property);
            }
        }
        return level.setBlock(pos, placed, update);
    }

    private static <T extends Comparable<T>> BlockState copy(BlockState target, BlockState source, Property<T> property) {
        return target.setValue(property, source.getValue(property));
    }
}

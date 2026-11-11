// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.predicate;
import java.util.function.Predicate;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.Property;
public class BlockStatePredicate implements Predicate<BlockState> {
    private final StateDefinition<Block, BlockState> definition;
    private final java.util.Map<Property<?>, Predicate<Object>> properties = new java.util.HashMap<>();
    public static final Predicate<BlockState> ANY = input -> true;
    private BlockStatePredicate(StateDefinition<Block, BlockState> definition) {
        this.definition = definition;
    }
    public static BlockStatePredicate forBlock(Block block) {
        return new BlockStatePredicate(block.getStateDefinition());
    }
    public boolean test(BlockState input) {
        if (input == null || !input.getBlock().equals(definition.getOwner())) {
            return false;
        }
        for (java.util.Map.Entry<Property<?>, Predicate<Object>> entry : properties.entrySet()) {
            if (!applies(input, entry.getKey(), entry.getValue())) {
                return false;
            }
        }
        return true;
    }
    protected <T extends Comparable<T>> boolean applies(BlockState input, Property<T> key, Predicate<Object> predicate) {
        return predicate.test(input.getValue(key));
    }
    public <V extends Comparable<V>> BlockStatePredicate where(Property<V> property, Predicate<Object> predicate) {
        if (!definition.getProperties().contains(property)) {
            throw new IllegalArgumentException(definition + " cannot support property " + property);
        }
        properties.put(property, predicate);
        return this;
    }
}

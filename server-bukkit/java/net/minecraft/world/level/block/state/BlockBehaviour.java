package net.minecraft.world.level.block.state;

import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import net.minecraft.core.TypedInstance;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.properties.Property;

public abstract class BlockBehaviour implements FeatureElement {
    public abstract static class BlockStateBase extends StateHolder<Block, BlockState> implements TypedInstance<Block> {
        private final int id;

        protected BlockStateBase(Block block, int id) {
            super(block);
            this.id = id;
        }

        public int stateId() {
            return id;
        }

        public Block getBlock() {
            return owner;
        }

        @Override
        public Collection<Property<?>> getProperties() {
            return List.of(owner.propertiesInOrder());
        }

        @Override
        public <T extends Comparable<T>> boolean hasProperty(Property<T> property) {
            return owner.indexOf(property) >= 0;
        }

        @Override
        public <T extends Comparable<T>> T getValue(Property<T> property) {
            int index = owner.indexOf(property);
            if (index < 0) {
                throw new IllegalArgumentException("Cannot get property " + property + " as it does not exist in " + owner);
            }
            return property.getPossibleValues().get(valueIndex(index));
        }

        @Override
        public <T extends Comparable<T>, V extends T> BlockState setValue(Property<T> property, V value) {
            int index = owner.indexOf(property);
            if (index < 0) {
                throw new IllegalArgumentException("Cannot set property " + property + " as it does not exist in " + owner);
            }
            int wanted = property.getPossibleValues().indexOf(value);
            if (wanted < 0) {
                throw new IllegalArgumentException("Cannot set property " + property + " to " + value + " on " + owner);
            }
            int offset = id - owner.firstState();
            int stride = owner.strideOf(index);
            return owner.stateAtOffset(offset + (wanted - valueIndex(index)) * stride);
        }

        @Override
        public Map<Property<?>, Comparable<?>> getValues() {
            Map<Property<?>, Comparable<?>> values = new LinkedHashMap<>();
            Property<?>[] properties = owner.propertiesInOrder();
            for (int i = 0; i < properties.length; i++) {
                values.put(properties[i], properties[i].getPossibleValues().get(valueIndex(i)));
            }
            return values;
        }

        private int valueIndex(int index) {
            Property<?> property = owner.propertiesInOrder()[index];
            return ((id - owner.firstState()) / owner.strideOf(index)) % property.getPossibleValues().size();
        }
    }
}

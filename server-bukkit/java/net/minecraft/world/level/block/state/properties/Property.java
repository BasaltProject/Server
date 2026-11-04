// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;

import java.util.List;
import java.util.Optional;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import java.util.stream.Stream;
import net.minecraft.world.level.block.state.StateHolder;

public abstract class Property<T extends Comparable<T>> {
    private final Class<T> clazz;
    private final String name;

    protected Property(String name, Class<T> clazz) {
        this.clazz = clazz;
        this.name = name;
    }

    public String getName() {
        return name;
    }

    public Class<T> getValueClass() {
        return clazz;
    }

    public abstract List<T> getPossibleValues();

    public abstract String getName(T value);

    public abstract Optional<T> getValue(String name);

    public abstract int getInternalIndex(T value);

    @Override
    public String toString() {
        return getClass().getSimpleName() + "{name=" + name + ", clazz=" + clazz + ", values=" + getPossibleValues() + "}";
    }
    public Value<T> value(T value) {
        return new Value<>(this, value);
    }
    public Value<T> value(StateHolder<?, ?> stateHolder) {
        return new Value<>(this, stateHolder.getValue(this));
    }
    public Stream<Value<T>> getAllValues() {
        return getPossibleValues().stream().map(this::value);
    }
    public Codec<T> codec() {
        return Codec.STRING.comapFlatMap(
            text -> getValue(text).map(DataResult::success)
                .orElseGet(() -> DataResult.error(() -> "Unable to read property: " + this + " with value: " + text)),
            this::getName);
    }
    public Codec<Value<T>> valueCodec() {
        return codec().xmap(this::value, Value::value);
    }
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        return o instanceof Property<?> other && clazz.equals(other.clazz) && name.equals(other.name);
    }
    public int hashCode() {
        return generateHashCode();
    }
    public int generateHashCode() {
        return 31 * clazz.hashCode() + name.hashCode();
    }
    public <U, S extends StateHolder<?, S>> DataResult<S> parseValue(DynamicOps<U> ops, S state, U value) {
        return ops.getStringValue(value).flatMap(text -> getValue(text).map(found -> DataResult.success(state.setValue(this, found)))
            .orElseGet(() -> DataResult.error(() -> "Unable to read property: " + this + " with value: " + text)));
    }
    public static record Value<T extends Comparable<T>>(Property<T> property, T value) {
        public String valueName() {
            return property.getName(value);
        }
    }
}

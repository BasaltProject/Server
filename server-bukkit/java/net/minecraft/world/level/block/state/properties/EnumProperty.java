// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;

import java.util.List;
import java.util.Optional;
import net.minecraft.util.StringRepresentable;
import org.basaltmc.bukkit.facade.PropertyPool;
import java.util.function.Predicate;

public final class EnumProperty<T extends Enum<T> & StringRepresentable> extends Property<T> {
    private final List<T> values;

    public EnumProperty(String name, Class<T> clazz, List<T> values) {
        super(name, clazz);
        this.values = values;
    }

    @SafeVarargs
    public static <T extends Enum<T> & StringRepresentable> EnumProperty<T> create(String name, Class<T> clazz,
            T... values) {
        List<T> list = List.of(values);
        List<String> names = list.stream().map(StringRepresentable::getSerializedName).toList();
        return PropertyPool.intern("enum:" + name + ":" + names, () -> new EnumProperty<>(name, clazz, list));
    }

    @Override
    public List<T> getPossibleValues() {
        return values;
    }

    @Override
    public String getName(T value) {
        return value.getSerializedName();
    }

    @Override
    public Optional<T> getValue(String name) {
        return values.stream().filter(value -> value.getSerializedName().equals(name)).findFirst();
    }

    @Override
    public int getInternalIndex(T value) {
        return values.indexOf(value);
    }

    @Override
    public Class<T> getValueClass() {
        return super.getValueClass();
    }
    public boolean equals(Object o) {
        return o instanceof EnumProperty<?> other && super.equals(o) && values.equals(other.values);
    }
    public int generateHashCode() {
        return 31 * super.generateHashCode() + values.hashCode();
    }
    public static <T extends Enum<T> & StringRepresentable> EnumProperty<T> create(String name, Class<T> clazz) {
        return create(name, clazz, clazz.getEnumConstants());
    }
}

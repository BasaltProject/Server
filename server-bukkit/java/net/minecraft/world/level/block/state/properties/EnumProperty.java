package net.minecraft.world.level.block.state.properties;

import java.util.List;
import java.util.Optional;
import net.minecraft.util.StringRepresentable;
import org.basaltmc.bukkit.facade.PropertyPool;

public final class EnumProperty<T extends Comparable<T> & StringRepresentable> extends Property<T> {
    private final List<T> values;

    public EnumProperty(String name, Class<T> clazz, List<T> values) {
        super(name, clazz);
        this.values = values;
    }

    @SafeVarargs
    public static <T extends Comparable<T> & StringRepresentable> EnumProperty<T> create(String name, Class<T> clazz,
            T... values) {
        List<T> list = List.of(values);
        return PropertyPool.intern("enum:" + name + ":" + list, () -> new EnumProperty<>(name, clazz, list));
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
}

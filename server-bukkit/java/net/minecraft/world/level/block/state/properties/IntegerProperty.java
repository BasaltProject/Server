package net.minecraft.world.level.block.state.properties;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import org.basaltmc.bukkit.facade.PropertyPool;

public final class IntegerProperty extends Property<Integer> {
    private final List<Integer> values;
    private final int min;

    public IntegerProperty(String name, int min, int max) {
        super(name, Integer.class);
        this.min = min;
        List<Integer> list = new ArrayList<>();
        for (int i = min; i <= max; i++) {
            list.add(i);
        }
        values = List.copyOf(list);
    }

    public static IntegerProperty create(String name, int min, int max) {
        return PropertyPool.intern("int:" + name + ":" + min + ":" + max, () -> new IntegerProperty(name, min, max));
    }

    @Override
    public List<Integer> getPossibleValues() {
        return values;
    }

    @Override
    public String getName(Integer value) {
        return value.toString();
    }

    @Override
    public Optional<Integer> getValue(String name) {
        try {
            int parsed = Integer.parseInt(name);
            return parsed >= min && parsed < min + values.size() ? Optional.of(parsed) : Optional.empty();
        } catch (NumberFormatException e) {
            return Optional.empty();
        }
    }

    @Override
    public int getInternalIndex(Integer value) {
        return value - min;
    }
}

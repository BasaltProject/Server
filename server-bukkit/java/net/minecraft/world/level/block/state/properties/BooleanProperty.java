package net.minecraft.world.level.block.state.properties;

import java.util.List;
import java.util.Optional;
import org.basaltmc.bukkit.facade.PropertyPool;

public final class BooleanProperty extends Property<Boolean> {
    private static final List<Boolean> kValues = List.of(true, false);

    public BooleanProperty(String name) {
        super(name, Boolean.class);
    }

    public static BooleanProperty create(String name) {
        return PropertyPool.intern("bool:" + name, () -> new BooleanProperty(name));
    }

    @Override
    public List<Boolean> getPossibleValues() {
        return kValues;
    }

    @Override
    public String getName(Boolean value) {
        return value.toString();
    }

    @Override
    public Optional<Boolean> getValue(String name) {
        return switch (name) {
            case "true" -> Optional.of(true);
            case "false" -> Optional.of(false);
            default -> Optional.empty();
        };
    }

    @Override
    public int getInternalIndex(Boolean value) {
        return value ? 0 : 1;
    }
}

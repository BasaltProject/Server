package org.basaltmc.bukkit.facade;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Supplier;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.level.block.state.properties.Property;

public final class PropertyPool {
    private static final Map<String, Object> kPool = new ConcurrentHashMap<>();
    private static final Set<String> kDirections = Set.of("down", "up", "north", "south", "west", "east");

    private PropertyPool() {}

    @SuppressWarnings("unchecked")
    public static <P> P intern(String key, Supplier<P> maker) {
        return (P) kPool.computeIfAbsent(key, ignored -> maker.get());
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static Property<?> of(String name, String[] values) {
        BlockStateProperties.init();
        if (values.length == 2 && values[0].equals("true") && values[1].equals("false")) {
            return BooleanProperty.create(name);
        }
        if (isRange(values)) {
            int last = Integer.parseInt(values[values.length - 1]);
            return IntegerProperty.create(name, Integer.parseInt(values[0]), last);
        }
        boolean directions = true;
        for (String value : values) {
            directions &= kDirections.contains(value);
        }
        if (directions) {
            List<Direction> list = new ArrayList<>();
            for (String value : values) {
                list.add(Direction.valueOf(value.toUpperCase(java.util.Locale.ROOT)));
            }
            return EnumProperty.create(name, Direction.class, list.toArray(new Direction[0]));
        }
        Object found = kPool.get("enum:" + name + ":" + java.util.Arrays.asList(values));
        if (found instanceof Property<?> property) {
            return property;
        }
        throw new IllegalStateException("No block state property " + name + " with values " + String.join("|", values));
    }

    private static boolean isRange(String[] values) {
        try {
            int first = Integer.parseInt(values[0]);
            for (int i = 1; i < values.length; i++) {
                if (Integer.parseInt(values[i]) != first + i) {
                    return false;
                }
            }
            return true;
        } catch (NumberFormatException e) {
            return false;
        }
    }
}

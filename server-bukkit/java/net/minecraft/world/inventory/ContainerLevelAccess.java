// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import java.util.Optional;
import java.util.function.BiConsumer;
import java.util.function.BiFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;

public interface ContainerLevelAccess {
    ContainerLevelAccess NULL = new ContainerLevelAccess() {
        @Override
        public <T> Optional<T> evaluate(BiFunction<Level, BlockPos, T> action) {
            return Optional.empty();
        }
    };

    static ContainerLevelAccess create(Level level, BlockPos pos) {
        return new ContainerLevelAccess() {
            @Override
            public <T> Optional<T> evaluate(BiFunction<Level, BlockPos, T> action) {
                return Optional.of(action.apply(level, pos));
            }
        };
    }

    <T> Optional<T> evaluate(BiFunction<Level, BlockPos, T> action);

    default <T> T evaluate(BiFunction<Level, BlockPos, T> action, T defaultValue) {
        return evaluate(action).orElse(defaultValue);
    }

    default void execute(BiConsumer<Level, BlockPos> action) {
        evaluate((level, pos) -> {
            action.accept(level, pos);
            return Boolean.TRUE;
        });
    }
}

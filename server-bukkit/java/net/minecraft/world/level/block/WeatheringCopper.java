// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import com.google.common.collect.BiMap;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.function.IntFunction;
import java.util.function.Supplier;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.block.state.BlockState;
public interface WeatheringCopper extends ChangeOverTimeBlock<WeatheringCopper.WeatherState> {
    Supplier<BiMap<Block, Block>> NEXT_BY_BLOCK = null;
    Supplier<BiMap<Block, Block>> PREVIOUS_BY_BLOCK = null;
    private static Optional<Block> shift(Block block, int step) {
        String path = block.blockKey().getPath();
        if (path.startsWith("waxed_")) {
            return Optional.empty();
        }
        String base = path;
        int stage = 0;
        for (String prefix : new String[] {"exposed_", "weathered_", "oxidized_"}) {
            if (path.startsWith(prefix)) {
                base = path.substring(prefix.length());
                stage = java.util.Arrays.asList("exposed_", "weathered_", "oxidized_").indexOf(prefix) + 1;
            }
        }
        if (base.equals("copper")) {
            base = "copper_block";
        }
        if (!base.contains("copper") && !base.contains("lightning_rod")) {
            return Optional.empty();
        }
        int target = stage + step;
        if (target < 0 || target > 3) {
            return Optional.empty();
        }
        String name = target == 0 ? base : new String[] {"exposed_", "weathered_", "oxidized_"}[target - 1]
            + (base.equals("copper_block") ? "copper" : base);
        return net.minecraft.core.registries.BuiltInRegistries.BLOCK.getOptional(
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }
    static Optional<Block> getPrevious(Block block) {
        return shift(block, -1);
    }
    static Block getFirst(Block block) {
        Block current = block;
        for (Optional<Block> previous = getPrevious(current); previous.isPresent(); previous = getPrevious(current)) {
            current = previous.get();
        }
        return current;
    }
    static Optional<BlockState> getPrevious(BlockState state) {
        return getPrevious(state.getBlock()).map(previous -> previous.withPropertiesOf(state));
    }
    static Optional<Block> getNext(Block block) {
        return shift(block, 1);
    }
    static BlockState getFirst(BlockState state) {
        return getFirst(state.getBlock()).withPropertiesOf(state);
    }
    default Optional<BlockState> getNext(BlockState state) {
        return getNext(state.getBlock()).map(next -> next.withPropertiesOf(state));
    }
    default float getChanceModifier() {
        return getAge() == WeatherState.UNAFFECTED ? 0.75F : 1.0F;
    }
    public static enum WeatherState implements StringRepresentable {
        UNAFFECTED, EXPOSED, WEATHERED, OXIDIZED;
        public static final IntFunction<WeatherState> BY_ID = i -> values()[Math.floorMod(i, values().length)];
        public static final Codec<WeatherState> CODEC = StringRepresentable.fromEnum(WeatherState::values);
        public static final StreamCodec<ByteBuf, WeatherState> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.idMapper(BY_ID, WeatherState::ordinal);
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public static void forEach(Consumer<WeatherState> consumer) {
            for (WeatherState state : values()) {
                consumer.accept(state);
            }
        }
        public WeatherState next() {
            return values()[Math.min(ordinal() + 1, values().length - 1)];
        }
        public WeatherState previous() {
            return values()[Math.max(ordinal() - 1, 0)];
        }
    }
}

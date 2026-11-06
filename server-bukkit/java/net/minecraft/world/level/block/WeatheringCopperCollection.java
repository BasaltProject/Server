// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import java.util.function.Function;
import net.minecraft.data.BlockFamily;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.block.state.BlockBehaviour;
import org.apache.commons.lang3.function.TriFunction;
public record WeatheringCopperCollection<T>(ByState<T> weathering, ByState<T> waxed) {
    public static final ByState<WeatheringCopper.WeatherState> STATES = new ByState<>(
        WeatheringCopper.WeatherState.UNAFFECTED, WeatheringCopper.WeatherState.EXPOSED,
        WeatheringCopper.WeatherState.WEATHERED, WeatheringCopper.WeatherState.OXIDIZED);
    public static final WeatheringCopperCollection<String> PREFIXES = new WeatheringCopperCollection<>(
        new ByState<>("", "exposed_", "weathered_", "oxidized_"),
        new ByState<>("waxed_", "waxed_exposed_", "waxed_weathered_", "waxed_oxidized_"));

    public static WeatheringCopperCollection<String> prefixWithState(WeatheringCopperCollection<String> ids) {
        return zipMap(PREFIXES, ids, (prefix, id) -> prefix + id);
    }

    public static WeatheringCopperCollection<String> create(String name) {
        return prefixWithState(new WeatheringCopperCollection<>(ByState.create(name), ByState.create(name)));
    }

    public static WeatheringCopperCollection<String> same(ByState<String> byState) {
        return new WeatheringCopperCollection<>(byState, byState);
    }

    public static <WaxedBlock extends Block, WeatheringBlock extends Block & WeatheringCopper, Id>
            WeatheringCopperCollection<Block> registerBlocks(WeatheringCopperCollection<Id> ids,
                TriFunction<Id, Function<BlockBehaviour.Properties, Block>, BlockBehaviour.Properties, Block> register,
                BiFunction<WeatheringCopper.WeatherState, BlockBehaviour.Properties, WaxedBlock> waxedBlockFactory,
                BiFunction<WeatheringCopper.WeatherState, BlockBehaviour.Properties, WeatheringBlock> weatheringFactory,
                Function<WeatheringCopper.WeatherState, BlockBehaviour.Properties> propertiesSupplier) {
        return ids.apply(
            weatheringIds -> ByState.zipMap(STATES, weatheringIds, (state, id) -> register.apply(id, p -> weatheringFactory.apply(state, p), propertiesSupplier.apply(state))),
            waxedIds -> ByState.zipMap(STATES, waxedIds, (state, id) -> register.apply(id, p -> waxedBlockFactory.apply(state, p), propertiesSupplier.apply(state))));
    }

    public static <Id> WeatheringCopperCollection<Item> registerItems(WeatheringCopperCollection<Id> ids,
            WeatheringCopperCollection<Block> blocks, BiFunction<Id, Block, Item> itemFactory) {
        return zipMap(ids, blocks, itemFactory);
    }

    public static WeatheringCopperCollection<BlockFamily> createFamily(
            BiFunction<String, WeatheringCopper.WeatherState, BlockFamily> waxedProvider,
            BiFunction<String, WeatheringCopper.WeatherState, BlockFamily> weatheringProvider) {
        return PREFIXES.apply(prefixes -> ByState.zipMap(prefixes, STATES, weatheringProvider),
            prefixes -> ByState.zipMap(prefixes, STATES, waxedProvider));
    }

    public List<T> asList() {
        List<T> all = new java.util.ArrayList<>();
        weathering.forEach(all::add);
        waxed.forEach(all::add);
        return all;
    }

    public void forEach(Consumer<T> consumer) {
        asList().forEach(consumer);
    }

    public <U> WeatheringCopperCollection<U> map(Function<T, U> mapper) {
        return new WeatheringCopperCollection<>(weathering.map(mapper), waxed.map(mapper));
    }

    public <U> WeatheringCopperCollection<U> apply(Function<ByState<T>, ByState<U>> mapper) {
        return new WeatheringCopperCollection<>(mapper.apply(weathering), mapper.apply(waxed));
    }

    public <U> WeatheringCopperCollection<U> apply(Function<ByState<T>, ByState<U>> weatheringMapper,
            Function<ByState<T>, ByState<U>> waxedMapper) {
        return new WeatheringCopperCollection<>(weatheringMapper.apply(weathering), waxedMapper.apply(waxed));
    }

    public void zipUnwaxedWaxed(BiConsumer<T, T> consumer) {
        ByState.zipApply(weathering, waxed, consumer);
    }

    public static <T, U> void zipApply(WeatheringCopperCollection<T> first, WeatheringCopperCollection<U> second,
            BiConsumer<T, U> consumer) {
        ByState.zipApply(first.weathering, second.weathering, consumer);
        ByState.zipApply(first.waxed, second.waxed, consumer);
    }

    public static <T, U, R> WeatheringCopperCollection<R> zipMap(WeatheringCopperCollection<T> first,
            WeatheringCopperCollection<U> second, BiFunction<T, U, R> operation) {
        return new WeatheringCopperCollection<>(ByState.zipMap(first.weathering, second.weathering, operation),
            ByState.zipMap(first.waxed, second.waxed, operation));
    }

    public static record ByState<T>(T unaffected, T exposed, T weathered, T oxidized) {
        public static <T> ByState<T> create(T value) {
            return new ByState<>(value, value, value, value);
        }

        public <U> ByState<U> map(Function<T, U> mapper) {
            return new ByState<>(mapper.apply(unaffected), mapper.apply(exposed), mapper.apply(weathered),
                mapper.apply(oxidized));
        }

        public T pick(WeatheringCopper.WeatherState state) {
            return switch (state) {
                case UNAFFECTED -> unaffected;
                case EXPOSED -> exposed;
                case WEATHERED -> weathered;
                case OXIDIZED -> oxidized;
            };
        }

        public void forEach(Consumer<T> consumer) {
            consumer.accept(unaffected);
            consumer.accept(exposed);
            consumer.accept(weathered);
            consumer.accept(oxidized);
        }

        public void progressMapping(BiConsumer<T, T> consumer) {
            consumer.accept(unaffected, exposed);
            consumer.accept(exposed, weathered);
            consumer.accept(weathered, oxidized);
        }

        static <T, U> void zipApply(ByState<T> first, ByState<U> second, BiConsumer<T, U> consumer) {
            consumer.accept(first.unaffected, second.unaffected);
            consumer.accept(first.exposed, second.exposed);
            consumer.accept(first.weathered, second.weathered);
            consumer.accept(first.oxidized, second.oxidized);
        }

        static <T, U, R> ByState<R> zipMap(ByState<T> first, ByState<U> second, BiFunction<T, U, R> operation) {
            return new ByState<>(operation.apply(first.unaffected, second.unaffected),
                operation.apply(first.exposed, second.exposed), operation.apply(first.weathered, second.weathered),
                operation.apply(first.oxidized, second.oxidized));
        }
    }
}

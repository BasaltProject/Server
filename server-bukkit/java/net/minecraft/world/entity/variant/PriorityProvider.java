// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.variant;
import com.mojang.serialization.Codec;
import java.util.Comparator;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.util.RandomSource;
public interface PriorityProvider<Context, Condition extends PriorityProvider.SelectorCondition<Context>> {
    List<Selector<Context, Condition>> selectors();
    static <C, T> Stream<T> select(Stream<T> entries, Function<T, PriorityProvider<C, ?>> extractor, C context) {
        List<UnpackedEntry<C, T>> unpacked = new java.util.ArrayList<>();
        entries.forEach(entry -> {
            PriorityProvider<C, ?> provider = extractor.apply(entry);
            for (Selector<C, ?> selector : provider.selectors()) {
                @SuppressWarnings("unchecked") SelectorCondition<C> condition = selector.condition().isPresent() ? (SelectorCondition<C>) selector.condition().get() : SelectorCondition.alwaysTrue();
                unpacked.add(new UnpackedEntry<>(entry, selector.priority(), condition));
            }
        });
        unpacked.sort(UnpackedEntry.HIGHEST_PRIORITY_FIRST);
        int highestMatched = Integer.MIN_VALUE;
        java.util.Iterator<UnpackedEntry<C, T>> iterator = unpacked.iterator();
        while (iterator.hasNext()) {
            UnpackedEntry<C, T> entry = iterator.next();
            if (entry.priority() < highestMatched) {
                iterator.remove();
            } else if (entry.condition().test(context)) {
                highestMatched = entry.priority();
            } else {
                iterator.remove();
            }
        }
        return unpacked.stream().map(UnpackedEntry::entry);
    }
    static <C, T> Optional<T> pick(Stream<T> entries, Function<T, PriorityProvider<C, ?>> extractor, RandomSource randomSource, C context) {
        List<T> selected = select(entries, extractor, context).toList();
        return selected.isEmpty() ? Optional.empty() : Optional.of(selected.get(randomSource.nextInt(selected.size())));
    }
    static <Context, Condition extends SelectorCondition<Context>> List<Selector<Context, Condition>> single(Condition check, int priority) {
        return List.of(new Selector<Context, Condition>(check, priority));
    }
    static <Context, Condition extends SelectorCondition<Context>> List<Selector<Context, Condition>> alwaysTrue(int priority) {
        return List.of(new Selector<Context, Condition>(priority));
    }
    public static record Selector<Context, Condition extends SelectorCondition<Context>>(Optional<Condition> condition, int priority) {
        public Selector(Condition condition, int priority) {
            this(Optional.of(condition), priority);
        }
        public Selector(int priority) {
            this(Optional.empty(), priority);
        }
        public static <Context, Condition extends SelectorCondition<Context>> Codec<Selector<Context, Condition>> codec(Codec<Condition> conditionCodec) {
            return com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
                conditionCodec.optionalFieldOf("condition").forGetter(Selector::condition),
                Codec.INT.fieldOf("priority").forGetter(Selector::priority)).apply(i, Selector::new));
        }
    }
    public interface SelectorCondition<C> extends Predicate<C> {
        static <C> SelectorCondition<C> alwaysTrue() {
            return context -> true;
        }
    }
    public static record UnpackedEntry<C, T>(T entry, int priority, SelectorCondition<C> condition) {
        public static final Comparator<UnpackedEntry<?, ?>> HIGHEST_PRIORITY_FIRST = Comparator.comparingInt((UnpackedEntry<?, ?> entry) -> entry.priority()).reversed();
    }
}

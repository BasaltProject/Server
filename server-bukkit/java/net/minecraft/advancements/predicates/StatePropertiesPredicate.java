// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;

import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.StateHolder;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.material.FluidState;

public record StatePropertiesPredicate(List<PropertyMatcher> properties) {
    public static final Codec<StatePropertiesPredicate> CODEC = Codec.unboundedMap(Codec.STRING, ValueMatcher.CODEC).xmap(
        map -> new StatePropertiesPredicate(map.entrySet().stream().map(e -> new PropertyMatcher(e.getKey(), e.getValue())).toList()),
        predicate -> {
            Map<String, ValueMatcher> out = new java.util.LinkedHashMap<>();
            predicate.properties.forEach(matcher -> out.put(matcher.name, matcher.valueMatcher));
            return out;
        });
    public static final StreamCodec<ByteBuf, StatePropertiesPredicate> STREAM_CODEC = PropertyMatcher.STREAM_CODEC.apply(ByteBufCodecs.list())
        .map(StatePropertiesPredicate::new, StatePropertiesPredicate::properties);

    public <S extends StateHolder<?, S>> boolean matches(StateDefinition<?, S> definition, S state) {
        for (PropertyMatcher matcher : properties) {
            if (!matcher.match(definition, state)) {
                return false;
            }
        }
        return true;
    }

    public boolean matches(BlockState state) {
        return matches(state.getBlock().getStateDefinition(), state);
    }

    public boolean matches(FluidState state) {
        return matches(state.getType().getStateDefinition(), state);
    }

    public Optional<String> checkState(StateDefinition<?, ?> states) {
        for (PropertyMatcher matcher : properties) {
            Optional<String> problem = matcher.checkState(states);
            if (problem.isPresent()) {
                return problem;
            }
        }
        return Optional.empty();
    }

    static record PropertyMatcher(String name, ValueMatcher valueMatcher) {
        public static final StreamCodec<ByteBuf, PropertyMatcher> STREAM_CODEC = StreamCodec.composite(
            ByteBufCodecs.STRING_UTF8, PropertyMatcher::name, ValueMatcher.STREAM_CODEC, PropertyMatcher::valueMatcher, PropertyMatcher::new);

        public <S extends StateHolder<?, S>> boolean match(StateDefinition<?, S> definition, S state) {
            Property<?> property = definition.getProperty(name);
            return property != null && valueMatcher.match(state, property);
        }

        public Optional<String> checkState(StateDefinition<?, ?> states) {
            return states.getProperty(name) == null ? Optional.of(name) : Optional.empty();
        }
    }

    interface ValueMatcher {
        Codec<ValueMatcher> CODEC = Codec.either(ExactMatcher.CODEC, RangedMatcher.CODEC).xmap(
            either -> either.map(exact -> (ValueMatcher) exact, ranged -> (ValueMatcher) ranged),
            matcher -> matcher instanceof ExactMatcher exact ? Either.left(exact) : Either.right((RangedMatcher) matcher));
        StreamCodec<ByteBuf, ValueMatcher> STREAM_CODEC = StreamCodec.of((buf, matcher) -> {
            buf.writeBoolean(matcher instanceof ExactMatcher);
            if (matcher instanceof ExactMatcher exact) {
                ExactMatcher.STREAM_CODEC.encode(buf, exact);
            } else {
                RangedMatcher.STREAM_CODEC.encode(buf, (RangedMatcher) matcher);
            }
        }, buf -> buf.readBoolean() ? ExactMatcher.STREAM_CODEC.decode(buf) : RangedMatcher.STREAM_CODEC.decode(buf));

        <T extends Comparable<T>> boolean match(StateHolder<?, ?> state, Property<T> property);
    }

    static record ExactMatcher(String value) implements ValueMatcher {
        public static final Codec<ExactMatcher> CODEC = Codec.STRING.xmap(ExactMatcher::new, ExactMatcher::value);
        public static final StreamCodec<ByteBuf, ExactMatcher> STREAM_CODEC = ByteBufCodecs.STRING_UTF8.map(ExactMatcher::new, ExactMatcher::value);

        @Override
        public <T extends Comparable<T>> boolean match(StateHolder<?, ?> state, Property<T> property) {
            Optional<T> wanted = property.getValue(value);
            return wanted.isPresent() && state.getValue(property).compareTo(wanted.get()) == 0;
        }
    }

    static record RangedMatcher(Optional<String> minValue, Optional<String> maxValue) implements ValueMatcher {
        public static final Codec<RangedMatcher> CODEC = RecordCodecBuilder.create(i -> i.group(
            Codec.STRING.optionalFieldOf("min").forGetter(RangedMatcher::minValue),
            Codec.STRING.optionalFieldOf("max").forGetter(RangedMatcher::maxValue)).apply(i, RangedMatcher::new));
        public static final StreamCodec<ByteBuf, RangedMatcher> STREAM_CODEC = StreamCodec.composite(
            ByteBufCodecs.optional(ByteBufCodecs.STRING_UTF8), RangedMatcher::minValue,
            ByteBufCodecs.optional(ByteBufCodecs.STRING_UTF8), RangedMatcher::maxValue, RangedMatcher::new);

        @Override
        public <T extends Comparable<T>> boolean match(StateHolder<?, ?> state, Property<T> property) {
            T actual = state.getValue(property);
            if (minValue.isPresent()) {
                Optional<T> low = property.getValue(minValue.get());
                if (low.isEmpty() || actual.compareTo(low.get()) < 0) {
                    return false;
                }
            }
            if (maxValue.isPresent()) {
                Optional<T> high = property.getValue(maxValue.get());
                return high.isPresent() && actual.compareTo(high.get()) <= 0;
            }
            return true;
        }
    }

    public static class Builder {
        private final List<PropertyMatcher> matchers = new ArrayList<>();

        private Builder() {}

        public static Builder properties() {
            return new Builder();
        }

        public Builder hasProperty(Property<?> property, String value) {
            matchers.add(new PropertyMatcher(property.getName(), new ExactMatcher(value)));
            return this;
        }

        public Builder hasProperty(Property<Integer> property, int value) {
            return hasProperty(property, Integer.toString(value));
        }

        public Builder hasProperty(Property<Boolean> property, boolean value) {
            return hasProperty(property, Boolean.toString(value));
        }

        public <T extends Comparable<T> & StringRepresentable> Builder hasProperty(Property<T> property, T value) {
            return hasProperty(property, value.getSerializedName());
        }

        public Optional<StatePropertiesPredicate> build() {
            return Optional.of(new StatePropertiesPredicate(List.copyOf(matchers)));
        }
    }
}

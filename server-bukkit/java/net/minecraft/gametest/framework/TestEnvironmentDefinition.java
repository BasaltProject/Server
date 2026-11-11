// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.Unit;
import net.minecraft.world.Difficulty;
import net.minecraft.world.attribute.EnvironmentAttributeSystem;
import net.minecraft.world.clock.WorldClock;
import net.minecraft.world.level.gamerules.GameRuleMap;
import net.minecraft.world.timeline.Timeline;

public interface TestEnvironmentDefinition<SavedDataType> {
    Codec<TestEnvironmentDefinition<?>> DIRECT_CODEC = BuiltInRegistries.TEST_ENVIRONMENT_DEFINITION_TYPE.byNameCodec().dispatch(TestEnvironmentDefinition::codec, codec -> codec);
    Codec<Holder<TestEnvironmentDefinition<?>>> CODEC = DIRECT_CODEC.xmap(Holder::direct, Holder::value);

    static MapCodec<? extends TestEnvironmentDefinition<?>> bootstrap(Registry<MapCodec<? extends TestEnvironmentDefinition<?>>> registry) {
        Registry.register(registry, Identifier.withDefaultNamespace("all_of"), AllOf.CODEC);
        Registry.register(registry, Identifier.withDefaultNamespace("game_rules"), SetGameRules.CODEC);
        Registry.register(registry, Identifier.withDefaultNamespace("time_of_day"), ClockTime.CODEC);
        Registry.register(registry, Identifier.withDefaultNamespace("weather"), Weather.CODEC);
        Registry.register(registry, Identifier.withDefaultNamespace("timelines"), Timelines.CODEC);
        Registry.register(registry, Identifier.withDefaultNamespace("difficulty"), SetDifficulty.CODEC);
        return Registry.register(registry, Identifier.withDefaultNamespace("function"), Functions.CODEC);
    }

    SavedDataType setup(ServerLevel level);

    void teardown(ServerLevel level, SavedDataType saveData);

    MapCodec<? extends TestEnvironmentDefinition<SavedDataType>> codec();

    static <T> Activation<T> activate(TestEnvironmentDefinition<T> environment, ServerLevel level) {
        return new Activation<>(environment.setup(level), environment, level);
    }

    public static class Activation<T> {
        private final T value;
        private final TestEnvironmentDefinition<T> definition;
        private final ServerLevel level;

        private Activation(T value, TestEnvironmentDefinition<T> definition, ServerLevel level) {
            this.value = value;
            this.definition = definition;
            this.level = level;
        }

        public void teardown() {
            definition.teardown(level, value);
        }
    }

    public static record Weather(Type weather) implements TestEnvironmentDefinition<Weather.Type> {
        public static final MapCodec<Weather> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(Type.CODEC.fieldOf("weather").forGetter(Weather::weather)).apply(i, Weather::new));

        @Override
        public Type setup(ServerLevel level) {
            Type previous = level.isThundering() ? Type.THUNDER : level.isRaining() ? Type.RAIN : Type.CLEAR;
            weather.apply(level);
            return previous;
        }

        @Override
        public void teardown(ServerLevel level, Type saveData) {
            saveData.apply(level);
        }

        @Override
        public MapCodec<Weather> codec() {
            return CODEC;
        }

        public static enum Type implements StringRepresentable {
            CLEAR, RAIN, THUNDER;

            public static final Codec<Type> CODEC = StringRepresentable.fromEnum(Type::values);

            public void apply(ServerLevel level) {
                level.getServer().setWeatherParameters(this == CLEAR ? 1_000_000 : 0, this == CLEAR ? 0 : 1_000_000, this != CLEAR, this == THUNDER);
            }

            @Override
            public String getSerializedName() {
                return name().toLowerCase(java.util.Locale.ROOT);
            }
        }
    }

    public static record ClockTime(Holder<WorldClock> clock, int time) implements TestEnvironmentDefinition<Long> {
        public static final MapCodec<ClockTime> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            WorldClock.CODEC.fieldOf("clock").forGetter(ClockTime::clock),
            Codec.INT.fieldOf("time").forGetter(ClockTime::time)).apply(i, ClockTime::new));

        @Override
        public Long setup(ServerLevel level) {
            long previous = level.clockManager().getInstance(clock).totalTicks();
            level.clockManager().setTotalTicks(clock, time);
            return previous;
        }

        @Override
        public void teardown(ServerLevel level, Long saveData) {
            level.clockManager().setTotalTicks(clock, saveData);
        }

        @Override
        public MapCodec<ClockTime> codec() {
            return CODEC;
        }
    }

    public static record SetGameRules(GameRuleMap gameRulesMap) implements TestEnvironmentDefinition<GameRuleMap> {
        public static final MapCodec<SetGameRules> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            GameRuleMap.CODEC.fieldOf("rules").forGetter(SetGameRules::gameRulesMap)).apply(i, SetGameRules::new));

        @Override
        public GameRuleMap setup(ServerLevel level) {
            GameRuleMap previous = GameRuleMap.of();
            for (var rule : gameRulesMap.keySet()) {
                copyRule(level, previous, rule);
            }
            level.getGameRules().setAll(gameRulesMap, level.getServer());
            return previous;
        }

        private static <T> void copyRule(ServerLevel level, GameRuleMap into, net.minecraft.world.level.gamerules.GameRule<T> rule) {
            into.set(rule, level.getGameRules().get(rule));
        }

        @Override
        public void teardown(ServerLevel level, GameRuleMap saveData) {
            level.getGameRules().setAll(saveData, level.getServer());
        }

        @Override
        public MapCodec<SetGameRules> codec() {
            return CODEC;
        }
    }

    public static record Functions(Optional<Identifier> setupFunction, Optional<Identifier> teardownFunction) implements TestEnvironmentDefinition<Unit> {
        public static final MapCodec<Functions> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            Identifier.CODEC.optionalFieldOf("setup").forGetter(Functions::setupFunction),
            Identifier.CODEC.optionalFieldOf("teardown").forGetter(Functions::teardownFunction)).apply(i, Functions::new));

        private static void run(ServerLevel level, Identifier id) {
            level.getServer().getFunctions().get(id).ifPresent(function ->
                level.getServer().getFunctions().execute(function, level.getServer().createCommandSourceStack().withLevel(level).withSuppressedOutput()));
        }

        @Override
        public Unit setup(ServerLevel level) {
            setupFunction.ifPresent(id -> run(level, id));
            return Unit.INSTANCE;
        }

        @Override
        public void teardown(ServerLevel level, Unit saveData) {
            teardownFunction.ifPresent(id -> run(level, id));
        }

        @Override
        public MapCodec<Functions> codec() {
            return CODEC;
        }
    }

    public static record AllOf(List<Holder<TestEnvironmentDefinition<?>>> definitions) implements TestEnvironmentDefinition<List<? extends Activation<?>>> {
        public static final MapCodec<AllOf> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            TestEnvironmentDefinition.CODEC.listOf().fieldOf("definitions").forGetter(AllOf::definitions)).apply(i, AllOf::new));

        public AllOf(TestEnvironmentDefinition<?>... defs) {
            this(java.util.Arrays.stream(defs).map(def -> Holder.<TestEnvironmentDefinition<?>>direct(def)).toList());
        }

        @Override
        public List<? extends Activation<?>> setup(ServerLevel level) {
            List<Activation<?>> active = new ArrayList<>();
            for (Holder<TestEnvironmentDefinition<?>> definition : definitions) {
                active.add(activate(definition.value(), level));
            }
            return active;
        }

        @Override
        public void teardown(ServerLevel level, List<? extends Activation<?>> activations) {
            for (int i = activations.size() - 1; i >= 0; i--) {
                activations.get(i).teardown();
            }
        }

        @Override
        public MapCodec<AllOf> codec() {
            return CODEC;
        }
    }

    public static record Timelines(List<Holder<Timeline>> timelines) implements TestEnvironmentDefinition<EnvironmentAttributeSystem> {
        public static final MapCodec<Timelines> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            Timeline.CODEC.listOf().fieldOf("timelines").forGetter(Timelines::timelines)).apply(i, Timelines::new));

        @Override
        public EnvironmentAttributeSystem setup(ServerLevel level) {
            EnvironmentAttributeSystem.Builder builder = EnvironmentAttributeSystem.builder().addDefaultLayers(level);
            timelines.forEach(timeline -> builder.addTimelineLayer(timeline, level.clockManager()));
            return level.setEnvironmentAttributes(builder.build());
        }

        @Override
        public void teardown(ServerLevel level, EnvironmentAttributeSystem saveData) {
            level.setEnvironmentAttributes(saveData);
        }

        @Override
        public MapCodec<Timelines> codec() {
            return CODEC;
        }
    }

    public static record SetDifficulty(Difficulty difficulty) implements TestEnvironmentDefinition<Difficulty> {
        public static final MapCodec<SetDifficulty> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            Difficulty.CODEC.fieldOf("difficulty").forGetter(SetDifficulty::difficulty)).apply(i, SetDifficulty::new));

        @Override
        public Difficulty setup(ServerLevel level) {
            Difficulty previous = level.getDifficulty();
            level.getServer().setDifficulty(difficulty, true);
            return previous;
        }

        @Override
        public void teardown(ServerLevel level, Difficulty saveData) {
            level.getServer().setDifficulty(saveData, true);
        }

        @Override
        public MapCodec<SetDifficulty> codec() {
            return CODEC;
        }
    }
}

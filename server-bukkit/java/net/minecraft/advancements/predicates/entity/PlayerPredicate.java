// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import it.unimi.dsi.fastutil.objects.Object2BooleanMap;
import it.unimi.dsi.fastutil.objects.Object2BooleanMaps;
import it.unimi.dsi.fastutil.objects.Object2BooleanOpenHashMap;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.advancements.AdvancementProgress;
import net.minecraft.advancements.CriterionProgress;
import net.minecraft.advancements.predicates.FoodPredicate;
import net.minecraft.advancements.predicates.GameTypePredicate;
import net.minecraft.advancements.predicates.InputPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.PlayerAdvancements;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.stats.Stat;
import net.minecraft.stats.StatType;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.phys.Vec3;
import org.basaltmc.bukkit.facade.HolderRefs;
public record PlayerPredicate(MinMaxBounds.Ints level, FoodPredicate food, GameTypePredicate gameType, List<StatMatcher> stats, Object2BooleanMap<ResourceKey<Recipe<?>>> recipes, Map<Identifier, AdvancementPredicate> advancements, Optional<EntityPredicate> lookingAt, Optional<InputPredicate> input) implements EntitySubPredicate {
    public static final int LOOKING_AT_RANGE = 100;
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static final Codec<Object2BooleanMap<ResourceKey<Recipe<?>>>> kRecipes = ExtraCodecs.object2BooleanMap(
        Identifier.CODEC.xmap(id -> (ResourceKey<Recipe<?>>) (ResourceKey) ResourceKey.create((ResourceKey) net.minecraft.core.registries.Registries.RECIPE, id),
            ResourceKey::identifier));
    public static final Codec<PlayerPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        MinMaxBounds.Ints.CODEC.optionalFieldOf("level", MinMaxBounds.Ints.ANY).forGetter(PlayerPredicate::level),
        FoodPredicate.CODEC.optionalFieldOf("food", FoodPredicate.ANY).forGetter(PlayerPredicate::food),
        GameTypePredicate.CODEC.optionalFieldOf("gamemode", GameTypePredicate.ANY).forGetter(PlayerPredicate::gameType),
        StatMatcher.CODEC.listOf().optionalFieldOf("stats", List.of()).forGetter(PlayerPredicate::stats),
        kRecipes.optionalFieldOf("recipes", Object2BooleanMaps.emptyMap()).forGetter(PlayerPredicate::recipes),
        Codec.unboundedMap(Identifier.CODEC, AdvancementPredicate.CODEC).optionalFieldOf("advancements", Map.of()).forGetter(PlayerPredicate::advancements),
        EntityPredicate.CODEC.optionalFieldOf("looking_at").forGetter(PlayerPredicate::lookingAt),
        InputPredicate.CODEC.optionalFieldOf("input").forGetter(PlayerPredicate::input)).apply(i, PlayerPredicate::new));
    @Override
    public boolean matches(Entity entity, ServerLevel serverLevel, Vec3 position) {
        if (!(entity instanceof ServerPlayer player) || !level.matches(player.experienceLevel) || !food.matches(player.getFoodData())
            || !gameType.matches(player.gameMode())) {
            return false;
        }
        for (StatMatcher stat : stats) {
            if (!stat.matches(player.getStats())) {
                return false;
            }
        }
        for (Object2BooleanMap.Entry<ResourceKey<Recipe<?>>> entry : recipes.object2BooleanEntrySet()) {
            if (player.getRecipeBook().contains(entry.getKey()) != entry.getBooleanValue()) {
                return false;
            }
        }
        if (!advancements.isEmpty()) {
            PlayerAdvancements progress = player.getAdvancements();
            for (Map.Entry<Identifier, AdvancementPredicate> entry : advancements.entrySet()) {
                AdvancementHolder holder = player.level().getServer().getAdvancements().get(entry.getKey());
                if (holder == null || !entry.getValue().test(progress.getOrStartProgress(holder))) {
                    return false;
                }
            }
        }
        if (lookingAt.isPresent()) {
            Vec3 from = player.getEyePosition();
            Vec3 view = player.getViewVector(1.0F);
            Vec3 to = from.add(view.x * LOOKING_AT_RANGE, view.y * LOOKING_AT_RANGE, view.z * LOOKING_AT_RANGE);
            Entity seen = org.basaltmc.bukkit.facade.EntityRays.firstEntity(player, from, to);
            if (seen == null || !lookingAt.get().matches(player, seen) || !player.hasLineOfSight(seen)) {
                return false;
            }
        }
        return input.isEmpty() || input.get().matches(player.getLastClientInput());
    }
    public interface AdvancementPredicate extends Predicate<AdvancementProgress> {
        Codec<AdvancementPredicate> CODEC = Codec.either(Done.CODEC, Criteria.CODEC).xmap(
            either -> either.map(done -> (AdvancementPredicate) done, criteria -> (AdvancementPredicate) criteria),
            predicate -> predicate instanceof Done done ? Either.<Done, Criteria>left(done) : Either.<Done, Criteria>right((Criteria) predicate));
    }
    public record Done(boolean state) implements AdvancementPredicate {
        public static final Codec<Done> CODEC = Codec.BOOL.xmap(Done::new, Done::state);
        @Override
        public boolean test(AdvancementProgress progress) {
            return progress.isDone() == state;
        }
    }
    public record Criteria(Object2BooleanMap<String> criterions) implements AdvancementPredicate {
        public static final Codec<Criteria> CODEC = ExtraCodecs.object2BooleanMap(Codec.STRING).xmap(Criteria::new, Criteria::criterions);
        @Override
        public boolean test(AdvancementProgress progress) {
            for (Object2BooleanMap.Entry<String> entry : criterions.object2BooleanEntrySet()) {
                CriterionProgress criterion = progress.getCriterion(entry.getKey());
                if (criterion == null || criterion.isDone() != entry.getBooleanValue()) {
                    return false;
                }
            }
            return true;
        }
    }
    public record StatMatcher(StatType<?> type, Holder<?> value, MinMaxBounds.Ints range) {
        public static final Codec<StatMatcher> CODEC = RecordCodecBuilder.create(i -> i.group(
            Identifier.CODEC.fieldOf("type").forGetter(matcher -> net.minecraft.core.registries.BuiltInRegistries.STAT_TYPE.getKey(matcher.type)),
            Identifier.CODEC.fieldOf("stat").forGetter(matcher -> matcher.value.unwrapKey().get().identifier()),
            MinMaxBounds.Ints.CODEC.optionalFieldOf("value", MinMaxBounds.Ints.ANY).forGetter(StatMatcher::range)).apply(i, StatMatcher::build));
        private static StatMatcher build(Identifier type, Identifier stat, MinMaxBounds.Ints range) {
            StatType<?> found = net.minecraft.core.registries.BuiltInRegistries.STAT_TYPE.getValue(type);
            return new StatMatcher(found, found == null ? null : HolderRefs.of(Object.class, stat), range);
        }
        @SuppressWarnings({"unchecked", "rawtypes"})
        public boolean matches(net.minecraft.stats.StatsCounter counter) {
            if (type == null || value == null) {
                return false;
            }
            Object key = ((net.minecraft.core.Registry) type.getRegistry()).getValue(value.unwrapKey().get().identifier());
            return key != null && range.matches(counter.getValue((StatType) type, key));
        }
    }
    public static class Builder {
        private MinMaxBounds.Ints level = MinMaxBounds.Ints.ANY;
        private FoodPredicate food = FoodPredicate.ANY;
        private GameTypePredicate gameType = GameTypePredicate.ANY;
        private final List<StatMatcher> stats = new ArrayList<>();
        private final Object2BooleanMap<ResourceKey<Recipe<?>>> recipes = new Object2BooleanOpenHashMap<>();
        private final Map<Identifier, AdvancementPredicate> advancements = new HashMap<>();
        private Optional<EntityPredicate> lookingAt = Optional.empty();
        private Optional<InputPredicate> input = Optional.empty();
        public static Builder player() {
            return new Builder();
        }
        public Builder setLevel(MinMaxBounds.Ints level) {
            this.level = level;
            return this;
        }
        public Builder setFood(FoodPredicate food) {
            this.food = food;
            return this;
        }
        public Builder addRecipe(ResourceKey<Recipe<?>> recipe, boolean present) {
            recipes.put(recipe, present);
            return this;
        }
        public Builder setGameType(GameTypePredicate gameType) {
            this.gameType = gameType;
            return this;
        }
        public Builder setLookingAt(EntityPredicate.Builder lookingAt) {
            this.lookingAt = Optional.of(lookingAt.build());
            return this;
        }
        public Builder checkAdvancementDone(Identifier advancement, boolean isDone) {
            advancements.put(advancement, new Done(isDone));
            return this;
        }
        public Builder checkAdvancementCriterions(Identifier advancement, Map<String, Boolean> criterions) {
            advancements.put(advancement, new Criteria(new Object2BooleanOpenHashMap<>(criterions)));
            return this;
        }
        public Builder hasInput(InputPredicate input) {
            this.input = Optional.of(input);
            return this;
        }
        public PlayerPredicate build() {
            return new PlayerPredicate(level, food, gameType, List.copyOf(stats), recipes, advancements, lookingAt, input);
        }
    }
}

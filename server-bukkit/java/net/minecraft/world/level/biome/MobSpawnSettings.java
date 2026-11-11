// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Map;
import java.util.Set;
import net.minecraft.util.random.WeightedList;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.MobCategory;
import com.google.common.collect.ImmutableMap;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.EnumMap;
import java.util.LinkedHashMap;
import net.minecraft.util.valueproviders.IntProviders;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.EntityTypes;
public class MobSpawnSettings {
    private static final org.slf4j.Logger kLog = org.slf4j.LoggerFactory.getLogger(MobSpawnSettings.class);
    public static final float DEFAULT_CREATURE_WORLD_GEN_SPAWN_PROBABILITY = 0.1F;
    public static final WeightedList<SpawnerData> EMPTY_MOB_LIST = WeightedList.of();
    public static final MobSpawnSettings EMPTY = new Builder().build();
    public static final MobSpawnSettings NO_SPAWNS = silenced();
    public static final Codec<MobSpawnSettings> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.simpleMap(MobCategory.CODEC, WeightedList.codec(SpawnerData.CODEC).promotePartial(message -> kLog.error("Spawn data: {}", message)),
            StringRepresentable.keys(MobCategory.values())).fieldOf("spawns_by_category").forGetter(settings -> settings.spawnsByCategory),
        Codec.simpleMap(BuiltInRegistries.ENTITY_TYPE.byNameCodec(), MobSpawnCost.CODEC, BuiltInRegistries.ENTITY_TYPE)
            .fieldOf("spawn_costs").forGetter(settings -> settings.mobSpawnCosts)).apply(i, MobSpawnSettings::new));
    private final Map<MobCategory, WeightedList<SpawnerData>> spawnsByCategory;
    private final Map<EntityType<?>, MobSpawnCost> mobSpawnCosts;
    private MobSpawnSettings(Map<MobCategory, WeightedList<SpawnerData>> spawnsByCategory, Map<EntityType<?>, MobSpawnCost> mobSpawnCosts) {
        this.spawnsByCategory = ImmutableMap.copyOf(spawnsByCategory);
        this.mobSpawnCosts = ImmutableMap.copyOf(mobSpawnCosts);
    }
    private static MobSpawnSettings silenced() {
        Builder builder = new Builder();
        for (MobCategory category : MobCategory.values()) {
            builder.noSpawns(category);
        }
        return builder.build();
    }
    public WeightedList<SpawnerData> getMobsToSpawn(MobCategory category) {
        return spawnsByCategory.getOrDefault(category, EMPTY_MOB_LIST);
    }
    public WeightedList<SpawnerData> getMobsInCategory(MobCategory category) {
        return spawnsByCategory.get(category);
    }
    public Set<MobCategory> definedCategories() {
        return spawnsByCategory.keySet();
    }
    public MobSpawnCost getMobSpawnCost(EntityType<?> type) {
        return mobSpawnCosts.get(type);
    }
    public Map<EntityType<?>, MobSpawnCost> allSpawnCosts() {
        return mobSpawnCosts;
    }
    public static record SpawnerData(EntityType<?> type, IntProvider count) {
        public static final MapCodec<SpawnerData> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            BuiltInRegistries.ENTITY_TYPE.byNameCodec().fieldOf("type").forGetter(SpawnerData::type),
            IntProviders.CODEC.fieldOf("count").forGetter(SpawnerData::count)).apply(i, SpawnerData::new));
        public SpawnerData(EntityType<?> type, IntProvider count) {
            this.type = type.getCategory() == MobCategory.MISC ? EntityTypes.PIG : type;
            this.count = count;
        }
        @Override
        public String toString() {
            return EntityType.getKey(type) + "*(" + count.minInclusive() + "-" + count.maxInclusive() + ") ("
                + BuiltInRegistries.INT_PROVIDER_TYPE.getKey(count.codec()) + ")";
        }
    }
    public static record MobSpawnCost(double energyBudget, double charge) {
        public static final Codec<MobSpawnCost> CODEC = RecordCodecBuilder.create(i -> i.group(
            Codec.DOUBLE.fieldOf("energy_budget").forGetter(cost -> cost.energyBudget),
            Codec.DOUBLE.fieldOf("charge").forGetter(cost -> cost.charge)).apply(i, MobSpawnCost::new));
    }
    public static class Builder {
        private final Map<MobCategory, WeightedList.Builder<SpawnerData>> spawns = new EnumMap<>(MobCategory.class);
        private final Map<EntityType<?>, MobSpawnCost> costs = new LinkedHashMap<>();
        private WeightedList.Builder<SpawnerData> slot(MobCategory category) {
            return spawns.computeIfAbsent(category, key -> WeightedList.builder());
        }
        public Builder addSpawn(EntityType<?> type, int weight, int minCount, int maxCount) {
            IntProvider count = minCount == maxCount ? new ConstantInt(minCount) : new UniformInt(minCount, maxCount);
            return addSpawn(type, type.getCategory(), weight, count);
        }
        public Builder addSpawn(EntityType<?> type, int weight, IntProvider count) {
            return addSpawn(type, type.getCategory(), weight, count);
        }
        @Deprecated
        public Builder addSpawn(EntityType<?> type, MobCategory category, int weight, IntProvider count) {
            slot(category).add(new SpawnerData(type, count), weight);
            return this;
        }
        public Builder addAllSpawns(MobCategory category, WeightedList<SpawnerData> spawns) {
            slot(category).addAll(spawns);
            return this;
        }
        public Builder noSpawns(MobCategory category) {
            spawns.put(category, WeightedList.builder());
            return this;
        }
        public Builder dontOverride(MobCategory category) {
            spawns.remove(category);
            return this;
        }
        public Builder addMobSpawnCost(EntityType<?> type, double charge, double energyBudget) {
            costs.put(type, new MobSpawnCost(energyBudget, charge));
            return this;
        }
        public Builder addAllCosts(Map<EntityType<?>, MobSpawnCost> costs) {
            this.costs.putAll(costs);
            return this;
        }
        public MobSpawnSettings build() {
            Map<MobCategory, WeightedList<SpawnerData>> built = new EnumMap<>(MobCategory.class);
            spawns.forEach((category, list) -> built.put(category, list.build()));
            return new MobSpawnSettings(built, costs);
        }
    }
}

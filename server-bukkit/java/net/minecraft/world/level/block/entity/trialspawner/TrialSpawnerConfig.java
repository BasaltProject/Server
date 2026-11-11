// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.trialspawner;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.nbt.CompoundTag;
import java.util.Optional;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.component.TypedEntityData;
import net.minecraft.world.level.SpawnData;
import net.minecraft.world.level.storage.loot.LootTable;
public record TrialSpawnerConfig(int spawnRange, float totalMobs, float simultaneousMobs, float totalMobsAddedPerPlayer, float simultaneousMobsAddedPerPlayer, int ticksBetweenSpawn, WeightedList<SpawnData> spawnPotentialsDefinition, WeightedList<ResourceKey<LootTable>> lootTablesToEject, ResourceKey<LootTable> itemsToDropWhenOminous) {
    private TrialSpawnerConfig withPotentials(WeightedList<SpawnData> potentials) {
        return new TrialSpawnerConfig(spawnRange, totalMobs, simultaneousMobs, totalMobsAddedPerPlayer, simultaneousMobsAddedPerPlayer,
            ticksBetweenSpawn, potentials, lootTablesToEject, itemsToDropWhenOminous);
    }
    public static final TrialSpawnerConfig DEFAULT = builder().build();
    public static final Codec<TrialSpawnerConfig> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.trialspawner.TrialSpawnerConfig.class, "DIRECT_CODEC");
    public static final Codec<Holder<TrialSpawnerConfig>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.trialspawner.TrialSpawnerConfig.class, "CODEC");
    public int calculateTargetTotalMobs(int additionalPlayers) {
        return (int) Math.floor(totalMobs + totalMobsAddedPerPlayer * additionalPlayers);
    }
    public int calculateTargetSimultaneousMobs(int additionalPlayers) {
        return (int) Math.floor(simultaneousMobs + simultaneousMobsAddedPerPlayer * additionalPlayers);
    }
    public long ticksBetweenItemSpawners() {
        return 160L;
    }
    public static Builder builder() {
        return new Builder();
    }
    public TrialSpawnerConfig withSpawning(EntityType<?> type) {
        CompoundTag tag = new CompoundTag();
        tag.putString("id", BuiltInRegistries.ENTITY_TYPE.getKey(type).toString());
        return withPotentials(WeightedList.of(new SpawnData(tag, Optional.empty(), Optional.empty())));
    }
    public TrialSpawnerConfig withSpawning(TypedEntityData<EntityType<?>> entityData) {
        return withPotentials(WeightedList.of(new SpawnData(entityData.copyTagWithoutId(), Optional.empty(), Optional.empty())));
    }
    public static class Builder {
        private int range = 4;
        private float total = 6.0F;
        private float simultaneous = 2.0F;
        private float totalPerPlayer = 2.0F;
        private float simultaneousPerPlayer = 1.0F;
        private int betweenSpawns = 40;
        private WeightedList<SpawnData> potentials = WeightedList.<SpawnData>of();
        private WeightedList<ResourceKey<LootTable>> ejected = WeightedList.<ResourceKey<LootTable>>builder()
            .add(BuiltInLootTables.SPAWNER_TRIAL_CHAMBER_CONSUMABLES).add(BuiltInLootTables.SPAWNER_TRIAL_CHAMBER_KEY).build();
        private ResourceKey<LootTable> ominousDrops = BuiltInLootTables.SPAWNER_TRIAL_ITEMS_TO_DROP_WHEN_OMINOUS;
        public Builder() {
        }
        public Builder spawnRange(int spawnRange) {
            this.range = spawnRange;
            return this;
        }
        public Builder totalMobs(float totalMobs) {
            this.total = totalMobs;
            return this;
        }
        public Builder simultaneousMobs(float simultaneousMobs) {
            this.simultaneous = simultaneousMobs;
            return this;
        }
        public Builder totalMobsAddedPerPlayer(float totalMobsAddedPerPlayer) {
            this.totalPerPlayer = totalMobsAddedPerPlayer;
            return this;
        }
        public Builder simultaneousMobsAddedPerPlayer(float simultaneousMobsAddedPerPlayer) {
            this.simultaneousPerPlayer = simultaneousMobsAddedPerPlayer;
            return this;
        }
        public Builder ticksBetweenSpawn(int ticksBetweenSpawn) {
            this.betweenSpawns = ticksBetweenSpawn;
            return this;
        }
        public Builder spawnPotentialsDefinition(WeightedList<SpawnData> spawnPotentialsDefinition) {
            this.potentials = spawnPotentialsDefinition;
            return this;
        }
        public Builder lootTablesToEject(WeightedList<ResourceKey<LootTable>> lootTablesToEject) {
            this.ejected = lootTablesToEject;
            return this;
        }
        public Builder itemsToDropWhenOminous(ResourceKey<LootTable> itemsToDropWhenOminous) {
            this.ominousDrops = itemsToDropWhenOminous;
            return this;
        }
        public TrialSpawnerConfig build() {
            return new TrialSpawnerConfig(range, total, simultaneous, totalPerPlayer, simultaneousPerPlayer, betweenSpawns, potentials, ejected, ominousDrops);


        }
    }
}

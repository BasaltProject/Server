// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.trialspawner;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.data.worldgen.BootstrapContext;
import java.util.Optional;
import net.minecraft.resources.ResourceKey;
public class TrialSpawnerConfigs {
    private static ResourceKey<TrialSpawnerConfig> configKey(String path) {
        return ResourceKey.create(Registries.TRIAL_SPAWNER_CONFIG, Identifier.withDefaultNamespace(path));
    }


    public static void bootstrap(BootstrapContext<TrialSpawnerConfig> context) {
        for (String id : org.basaltmc.bukkit.WorldNatives.registryIds("trial_spawner")) {
            byte[] bytes = org.basaltmc.bukkit.LeftoverNatives.registryData("trial_spawner", id);
            if (bytes != null) {
                context.register(ResourceKey.create(Registries.TRIAL_SPAWNER_CONFIG, Identifier.parse(id)),
                    read(org.basaltmc.bukkit.facade.NbtCodec.fromAnonymous(bytes)));
            }
        }
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static ResourceKey<net.minecraft.world.level.storage.loot.LootTable> lootKey(String id) {
        return (ResourceKey) ResourceKey.create((ResourceKey) Registries.LOOT_TABLE, Identifier.parse(id));
    }

    private static TrialSpawnerConfig read(net.minecraft.nbt.CompoundTag tag) {
        TrialSpawnerConfig.Builder builder = TrialSpawnerConfig.builder();
        builder.spawnRange(tag.getIntOr("spawn_range", 4)).totalMobs(tag.getFloatOr("total_mobs", 6.0F))
            .simultaneousMobs(tag.getFloatOr("simultaneous_mobs", 2.0F))
            .totalMobsAddedPerPlayer(tag.getFloatOr("total_mobs_added_per_player", 2.0F))
            .simultaneousMobsAddedPerPlayer(tag.getFloatOr("simultaneous_mobs_added_per_player", 1.0F))
            .ticksBetweenSpawn(tag.getIntOr("ticks_between_spawn", 40));
        java.util.List<net.minecraft.util.random.Weighted<net.minecraft.world.level.SpawnData>> spawns = new java.util.ArrayList<>();
        net.minecraft.nbt.ListTag potentials = tag.getListOrEmpty("spawn_potentials");
        for (int i = 0; i < potentials.size(); i++) {
            net.minecraft.nbt.CompoundTag entry = potentials.getCompoundOrEmpty(i);
            net.minecraft.nbt.CompoundTag entity = entry.getCompoundOrEmpty("data").getCompoundOrEmpty("entity");
            spawns.add(new net.minecraft.util.random.Weighted<>(
                new net.minecraft.world.level.SpawnData(entity, Optional.empty(), Optional.empty()), entry.getIntOr("weight", 1)));
        }
        builder.spawnPotentialsDefinition(net.minecraft.util.random.WeightedList.of(spawns));
        net.minecraft.util.random.WeightedList.Builder<ResourceKey<net.minecraft.world.level.storage.loot.LootTable>> loot =
            net.minecraft.util.random.WeightedList.builder();
        net.minecraft.nbt.ListTag ejected = tag.getListOrEmpty("loot_tables_to_eject");
        for (int i = 0; i < ejected.size(); i++) {
            net.minecraft.nbt.CompoundTag entry = ejected.getCompoundOrEmpty(i);
            loot.add(lootKey(entry.getStringOr("data", "")), entry.getIntOr("weight", 1));
        }
        builder.lootTablesToEject(loot.build());
        tag.getString("items_to_drop_when_ominous")
            .ifPresent(id -> builder.itemsToDropWhenOminous(lootKey(id)));
        return builder.build();
    }
    static record Keys(ResourceKey<TrialSpawnerConfig> normal, ResourceKey<TrialSpawnerConfig> ominous) {
        public static Keys of(String id) {
            return new Keys(configKey(id + "/normal"), configKey(id + "/ominous"));
        }
    }
}

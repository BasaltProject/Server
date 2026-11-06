// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.JsonOps;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.BiConsumer;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.RegistrySynchronization;
import net.minecraft.core.registries.Registries;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.server.packs.resources.Resource;
import net.minecraft.server.packs.resources.ResourceManager;
import net.minecraft.server.packs.resources.ResourceProvider;
import net.minecraft.tags.TagNetworkSerialization;
import org.basaltmc.bukkit.facade.NamedCodec;

public class RegistryDataLoader {
    private RegistryDataLoader() {}

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static RegistryData<?> data(ResourceKey<? extends Registry<?>> key, String owner, String field) {
        return new RegistryData(key, new NamedCodec<>(owner, field));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static RegistryData<?> data(ResourceKey<? extends Registry<?>> key, String owner, String field, boolean nonEmpty) {
        return new RegistryData(key, new NamedCodec<>(owner, field), nonEmpty ? RegistryValidator.nonEmpty() : RegistryValidator.none());
    }

    public static final List<RegistryData<?>> WORLD_REGISTRIES = List.of(
        data(Registries.DIMENSION_TYPE, "net.minecraft.world.level.dimension.DimensionType", "DIRECT_CODEC"),
        data(Registries.BIOME, "net.minecraft.world.level.biome.Biome", "DIRECT_CODEC"),
        data(Registries.CHAT_TYPE, "net.minecraft.network.chat.ChatType", "DIRECT_CODEC"),
        data(Registries.CARVER, "net.minecraft.world.level.levelgen.carver.WorldCarver", "DIRECT_CODEC"),
        data(Registries.FEATURE, "net.minecraft.world.level.levelgen.feature.Feature", "DIRECT_CODEC"),
        data(Registries.PLACED_FEATURE, "net.minecraft.world.level.levelgen.placement.PlacedFeature", "DIRECT_CODEC"),
        data(Registries.STRUCTURE, "net.minecraft.world.level.levelgen.structure.Structure", "DIRECT_CODEC"),
        data(Registries.STRUCTURE_SET, "net.minecraft.world.level.levelgen.structure.StructureSet", "DIRECT_CODEC"),
        data(Registries.PROCESSOR_LIST, "net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorType", "DIRECT_CODEC"),
        data(Registries.TEMPLATE_POOL, "net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool", "DIRECT_CODEC"),
        data(Registries.NOISE_SETTINGS, "net.minecraft.world.level.levelgen.NoiseGeneratorSettings", "DIRECT_CODEC"),
        data(Registries.NOISE, "net.minecraft.world.level.levelgen.synth.NormalNoise", "DIRECT_CODEC"),
        data(Registries.DENSITY_FUNCTION, "net.minecraft.world.level.levelgen.densityfunction.DensityFunctions", "DIRECT_CODEC"),
        data(Registries.MATERIAL_RULE, "net.minecraft.world.level.levelgen.material.rule.MaterialRule", "DIRECT_CODEC"),
        data(Registries.MATERIAL_CONDITION, "net.minecraft.world.level.levelgen.material.condition.MaterialCondition", "DIRECT_CODEC"),
        data(Registries.WORLD_PRESET, "net.minecraft.world.level.levelgen.presets.WorldPreset", "DIRECT_CODEC"),
        data(Registries.FLAT_LEVEL_GENERATOR_PRESET, "net.minecraft.world.level.levelgen.flat.FlatLevelGeneratorPreset", "DIRECT_CODEC"),
        data(Registries.TRIM_PATTERN, "net.minecraft.world.item.equipment.trim.TrimPattern", "DIRECT_CODEC"),
        data(Registries.TRIM_MATERIAL, "net.minecraft.world.item.equipment.trim.TrimMaterial", "DIRECT_CODEC"),
        data(Registries.TRIAL_SPAWNER_CONFIG, "net.minecraft.world.level.block.entity.trialspawner.TrialSpawnerConfig", "DIRECT_CODEC"),
        data(Registries.WOLF_VARIANT, "net.minecraft.world.entity.animal.wolf.WolfVariant", "DIRECT_CODEC", true),
        data(Registries.WOLF_SOUND_VARIANT, "net.minecraft.world.entity.animal.wolf.WolfSoundVariant", "DIRECT_CODEC", true),
        data(Registries.PIG_VARIANT, "net.minecraft.world.entity.animal.pig.PigVariant", "DIRECT_CODEC", true),
        data(Registries.PIG_SOUND_VARIANT, "net.minecraft.world.entity.animal.pig.PigSoundVariant", "DIRECT_CODEC", true),
        data(Registries.FROG_VARIANT, "net.minecraft.world.entity.animal.frog.FrogVariant", "DIRECT_CODEC", true),
        data(Registries.CAT_VARIANT, "net.minecraft.world.entity.animal.feline.CatVariant", "DIRECT_CODEC", true),
        data(Registries.CAT_SOUND_VARIANT, "net.minecraft.world.entity.animal.feline.CatSoundVariant", "DIRECT_CODEC", true),
        data(Registries.COW_VARIANT, "net.minecraft.world.entity.animal.cow.CowVariant", "DIRECT_CODEC", true),
        data(Registries.COW_SOUND_VARIANT, "net.minecraft.world.entity.animal.cow.CowSoundVariant", "DIRECT_CODEC", true),
        data(Registries.SULFUR_CUBE_ARCHETYPE, "net.minecraft.world.entity.SulfurCubeArchetype", "DIRECT_CODEC"),
        data(Registries.CHICKEN_VARIANT, "net.minecraft.world.entity.animal.chicken.ChickenVariant", "DIRECT_CODEC", true),
        data(Registries.CHICKEN_SOUND_VARIANT, "net.minecraft.world.entity.animal.chicken.ChickenSoundVariant", "DIRECT_CODEC", true),
        data(Registries.ZOMBIE_NAUTILUS_VARIANT, "net.minecraft.world.entity.animal.nautilus.ZombieNautilusVariant", "DIRECT_CODEC", true),
        data(Registries.PAINTING_VARIANT, "net.minecraft.world.entity.decoration.painting.PaintingVariant", "DIRECT_CODEC", true),
        data(Registries.DAMAGE_TYPE, "net.minecraft.world.damagesource.DamageType", "DIRECT_CODEC"),
        data(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST, "net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList", "DIRECT_CODEC"),
        data(Registries.BANNER_PATTERN, "net.minecraft.world.level.block.entity.BannerPattern", "DIRECT_CODEC"),
        data(Registries.ENCHANTMENT, "net.minecraft.world.item.enchantment.Enchantment", "DIRECT_CODEC"),
        data(Registries.ENCHANTMENT_PROVIDER, "net.minecraft.world.item.enchantment.providers.EnchantmentProvider", "DIRECT_CODEC"),
        data(Registries.JUKEBOX_SONG, "net.minecraft.world.item.JukeboxSong", "DIRECT_CODEC"),
        data(Registries.INSTRUMENT, "net.minecraft.world.item.Instrument", "DIRECT_CODEC"),
        data(Registries.TEST_ENVIRONMENT, "net.minecraft.gametest.framework.TestEnvironmentDefinition", "DIRECT_CODEC"),
        data(Registries.TEST_INSTANCE, "net.minecraft.gametest.framework.GameTestInstance", "DIRECT_CODEC"),
        data(Registries.DIALOG, "net.minecraft.server.dialog.Dialog", "DIRECT_CODEC"),
        data(Registries.WORLD_CLOCK, "net.minecraft.world.clock.WorldClock", "DIRECT_CODEC"),
        data(Registries.VILLAGER_TRADE, "net.minecraft.world.item.trading.VillagerTrade", "CODEC"),
        data(Registries.TRADE_SET, "net.minecraft.world.item.trading.TradeSet", "CODEC"),
        data(Registries.DECORATED_POT_PATTERN, "net.minecraft.world.level.block.entity.DecoratedPotPattern", "CODEC"),
        data(Registries.BLOCK_TRANSFORMER, "net.minecraft.core.component.BlockTransformer", "DIRECT_CODEC"),
        data(Registries.BLOCK_STATE_PROVIDER, "net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider", "DIRECT_CODEC"));
    public static final List<RegistryData<?>> DIMENSION_REGISTRIES = List.of(
        data(Registries.LEVEL_STEM, "net.minecraft.world.level.dimension.LevelStem", "CODEC"));
    public static final List<RegistryData<?>> RELOADABLE_REGISTRIES = List.of(
        data(Registries.LOOT_TABLE, "net.minecraft.world.level.storage.loot.LootTable", "DIRECT_CODEC"),
        data(Registries.PREDICATE, "net.minecraft.world.level.storage.loot.predicates.LootItemCondition", "DIRECT_CODEC"),
        data(Registries.CONTEXT_FLOAT_PROVIDER, "net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders", "DIRECT_CODEC"),
        data(Registries.CONTEXT_INT_PROVIDER, "net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders", "DIRECT_CODEC"),
        data(Registries.ITEM_MODIFIER, "net.minecraft.world.level.storage.loot.functions.LootItemFunctions", "TYPED_CODEC"),
        data(Registries.SLOT_SOURCE, "net.minecraft.world.item.slot.SlotSources", "TYPED_CODEC"),
        data(Registries.ADVANCEMENT, "net.minecraft.advancements.Advancement", "CODEC"),
        data(Registries.RECIPE, "net.minecraft.world.item.crafting.Recipe", "DIRECT_CODEC"));
    public static final List<RegistryData<?>> SYNCHRONIZED_REGISTRIES = List.of(
        data(Registries.BIOME, "net.minecraft.world.level.biome.Biome", "NETWORK_CODEC"),
        data(Registries.CHAT_TYPE, "net.minecraft.network.chat.ChatType", "DIRECT_CODEC"),
        data(Registries.TRIM_PATTERN, "net.minecraft.world.item.equipment.trim.TrimPattern", "DIRECT_CODEC"),
        data(Registries.TRIM_MATERIAL, "net.minecraft.world.item.equipment.trim.TrimMaterial", "DIRECT_CODEC"),
        data(Registries.WOLF_VARIANT, "net.minecraft.world.entity.animal.wolf.WolfVariant", "NETWORK_CODEC", true),
        data(Registries.WOLF_SOUND_VARIANT, "net.minecraft.world.entity.animal.wolf.WolfSoundVariant", "NETWORK_CODEC", true),
        data(Registries.PIG_VARIANT, "net.minecraft.world.entity.animal.pig.PigVariant", "NETWORK_CODEC", true),
        data(Registries.PIG_SOUND_VARIANT, "net.minecraft.world.entity.animal.pig.PigSoundVariant", "NETWORK_CODEC", true),
        data(Registries.FROG_VARIANT, "net.minecraft.world.entity.animal.frog.FrogVariant", "NETWORK_CODEC", true),
        data(Registries.CAT_VARIANT, "net.minecraft.world.entity.animal.feline.CatVariant", "NETWORK_CODEC", true),
        data(Registries.CAT_SOUND_VARIANT, "net.minecraft.world.entity.animal.feline.CatSoundVariant", "NETWORK_CODEC", true),
        data(Registries.COW_SOUND_VARIANT, "net.minecraft.world.entity.animal.cow.CowSoundVariant", "DIRECT_CODEC", true),
        data(Registries.COW_VARIANT, "net.minecraft.world.entity.animal.cow.CowVariant", "NETWORK_CODEC", true),
        data(Registries.CHICKEN_SOUND_VARIANT, "net.minecraft.world.entity.animal.chicken.ChickenSoundVariant", "DIRECT_CODEC", true),
        data(Registries.CHICKEN_VARIANT, "net.minecraft.world.entity.animal.chicken.ChickenVariant", "NETWORK_CODEC", true),
        data(Registries.ZOMBIE_NAUTILUS_VARIANT, "net.minecraft.world.entity.animal.nautilus.ZombieNautilusVariant", "NETWORK_CODEC", true),
        data(Registries.PAINTING_VARIANT, "net.minecraft.world.entity.decoration.painting.PaintingVariant", "DIRECT_CODEC", true),
        data(Registries.SULFUR_CUBE_ARCHETYPE, "net.minecraft.world.entity.SulfurCubeArchetype", "DIRECT_CODEC"),
        data(Registries.DIMENSION_TYPE, "net.minecraft.world.level.dimension.DimensionType", "NETWORK_CODEC"),
        data(Registries.DAMAGE_TYPE, "net.minecraft.world.damagesource.DamageType", "DIRECT_CODEC"),
        data(Registries.BANNER_PATTERN, "net.minecraft.world.level.block.entity.BannerPattern", "DIRECT_CODEC"),
        data(Registries.ENCHANTMENT, "net.minecraft.world.item.enchantment.Enchantment", "DIRECT_CODEC"),
        data(Registries.JUKEBOX_SONG, "net.minecraft.world.item.JukeboxSong", "DIRECT_CODEC"),
        data(Registries.INSTRUMENT, "net.minecraft.world.item.Instrument", "DIRECT_CODEC"),
        data(Registries.TEST_ENVIRONMENT, "net.minecraft.gametest.framework.TestEnvironmentDefinition", "DIRECT_CODEC"),
        data(Registries.TEST_INSTANCE, "net.minecraft.gametest.framework.GameTestInstance", "DIRECT_CODEC"),
        data(Registries.DIALOG, "net.minecraft.server.dialog.Dialog", "DIRECT_CODEC"),
        data(Registries.WORLD_CLOCK, "net.minecraft.world.clock.WorldClock", "DIRECT_CODEC"),
        data(Registries.TIMELINE, "net.minecraft.world.timeline.Timeline", "NETWORK_CODEC"),
        data(Registries.DECORATED_POT_PATTERN, "net.minecraft.world.level.block.entity.DecoratedPotPattern", "CODEC"),
        data(Registries.BLOCK_TRANSFORMER, "net.minecraft.core.component.BlockTransformer", "DIRECT_CODEC"),
        data(Registries.BLOCK_STATE_PROVIDER, "net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider", "DIRECT_CODEC"));

    public static CompletableFuture<RegistryAccess.Frozen> load(ResourceManager resourceManager, List<HolderLookup.RegistryLookup<?>> contextRegistries, List<RegistryData<?>> registriesToLoad, Executor executor) {
        return CompletableFuture.supplyAsync(() -> {
            Map<ResourceKey<?>, Exception> errors = new LinkedHashMap<>();
            List<Registry<?>> loaded = new java.util.ArrayList<>();
            for (RegistryData<?> data : registriesToLoad) {
                loaded.add(fromFiles(data, resourceManager, errors));
            }
            return finish(loaded, errors);
        }, executor);
    }

    public static CompletableFuture<RegistryAccess.Frozen> load(Map<ResourceKey<? extends Registry<?>>, NetworkedRegistryData> entries, ResourceProvider knownDataSource, List<HolderLookup.RegistryLookup<?>> contextRegistries, List<RegistryData<?>> registriesToLoad, Executor executor) {
        return CompletableFuture.supplyAsync(() -> {
            Map<ResourceKey<?>, Exception> errors = new LinkedHashMap<>();
            List<Registry<?>> loaded = new java.util.ArrayList<>();
            for (RegistryData<?> data : registriesToLoad) {
                NetworkedRegistryData sent = entries.get(data.key());
                if (sent != null) {
                    loaded.add(fromNetwork(data, sent, errors));
                }
            }
            return finish(loaded, errors);
        }, executor);
    }

    private static RegistryAccess.Frozen finish(List<Registry<?>> loaded, Map<ResourceKey<?>, Exception> errors) {
        if (!errors.isEmpty()) {
            IllegalStateException failure = new IllegalStateException("Failed to load registries due to errors");
            errors.values().forEach(failure::addSuppressed);
            throw failure;
        }
        return new RegistryAccess.ImmutableRegistryAccess(loaded).freeze();
    }

    private static <T> Registry<T> fromFiles(RegistryData<T> data, ResourceManager manager, Map<ResourceKey<?>, Exception> errors) {
        MappedRegistry<T> registry = new MappedRegistry<>(data.key());
        FileToIdConverter lister = FileToIdConverter.registry(data.key());
        for (Map.Entry<Identifier, Resource> entry : lister.listMatchingResources(manager).entrySet()) {
            Identifier id = lister.fileToId(entry.getKey());
            ResourceKey<T> key = ResourceKey.create(data.key(), id);
            try (java.io.Reader reader = entry.getValue().openAsReader()) {
                com.google.gson.JsonElement json = com.google.gson.JsonParser.parseReader(reader);
                DataResult<T> parsed = data.elementCodec().parse(JsonOps.INSTANCE, json);
                registry.register(key, parsed.getOrThrow());
            } catch (Exception failure) {
                errors.put(key, failure);
            }
        }
        data.validator().validate(registry, errors);
        registry.freeze();
        return registry;
    }

    private static <T> Registry<T> fromNetwork(RegistryData<T> data, NetworkedRegistryData sent, Map<ResourceKey<?>, Exception> errors) {
        MappedRegistry<T> registry = new MappedRegistry<>(data.key());
        for (RegistrySynchronization.PackedRegistryEntry element : sent.elements()) {
            ResourceKey<T> key = ResourceKey.create(data.key(), element.id());
            Optional<Tag> contents = element.data();
            if (contents.isEmpty()) {
                errors.put(key, new IllegalStateException("The element is known to the client and carries no data here"));
                continue;
            }
            try {
                registry.register(key, data.elementCodec().parse(NbtOps.INSTANCE, contents.get()).getOrThrow());
            } catch (Exception failure) {
                errors.put(key, failure);
            }
        }
        data.validator().validate(registry, errors);
        if (!sent.tags().isEmpty()) {
            registry.bindTags(net.minecraft.tags.TagLoader.loadTagsFromNetwork(sent.tags(), registry));
        }
        registry.freeze();
        return registry;
    }

    public static record RegistryData<T>(ResourceKey<? extends Registry<T>> key, Codec<T> elementCodec, RegistryValidator<T> validator) {
        private RegistryData(ResourceKey<? extends Registry<T>> key, Codec<T> elementCodec) {
            this(key, elementCodec, RegistryValidator.none());
        }

        public void runWithArguments(BiConsumer<ResourceKey<? extends Registry<T>>, Codec<T>> output) {
            output.accept(key, elementCodec);
        }
    }

    public static record NetworkedRegistryData(List<RegistrySynchronization.PackedRegistryEntry> elements, TagNetworkSerialization.NetworkPayload tags) {
    }

    interface LoaderFactory {
    }
}

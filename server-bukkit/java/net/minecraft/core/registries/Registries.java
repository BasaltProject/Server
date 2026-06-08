package net.minecraft.core.registries;

import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.material.Fluid;

public class Registries {
    public static final ResourceKey<Registry<Block>> BLOCK =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("block"));
    public static final ResourceKey<Registry<Item>> ITEM =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("item"));
    public static final ResourceKey<Registry<Fluid>> FLUID =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("fluid"));
    public static final ResourceKey<Registry<EntityType<?>>> ENTITY_TYPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("entity_type"));
    public static final ResourceKey<Registry<BlockEntityType<?>>> BLOCK_ENTITY_TYPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("block_entity_type"));
    public static final ResourceKey<Registry<Biome>> BIOME =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("biome"));
    public static final ResourceKey<Registry<Object>> DIMENSION_TYPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("dimension_type"));
    public static final ResourceKey<Registry<net.minecraft.world.level.Level>> DIMENSION =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("dimension"));
    public static final ResourceKey<Registry<Object>> SOUND_EVENT =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("sound_event"));
    public static final ResourceKey<Registry<Object>> FEATURE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/feature"));
    public static final ResourceKey<Registry<Object>> CONFIGURED_FEATURE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/configured_feature"));
    public static final ResourceKey<Registry<Object>> PLACED_FEATURE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/placed_feature"));
    public static final ResourceKey<Registry<Object>> STRUCTURE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/structure"));
    public static final ResourceKey<Registry<Object>> STRUCTURE_SET =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/structure_set"));
    public static final ResourceKey<Registry<Object>> NOISE_SETTINGS =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/noise_settings"));
    public static final ResourceKey<Registry<Object>> TEMPLATE_POOL =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/template_pool"));
    public static final ResourceKey<Registry<Object>> PROCESSOR_LIST =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/processor_list"));
    public static final ResourceKey<Registry<Object>> DAMAGE_TYPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("damage_type"));
    public static final ResourceKey<Registry<Object>> ENCHANTMENT =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("enchantment"));
    public static final ResourceKey<Registry<Object>> ATTRIBUTE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("attribute"));
    public static final ResourceKey<Registry<Object>> MOB_EFFECT =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("mob_effect"));
    public static final ResourceKey<Registry<Object>> PARTICLE_TYPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("particle_type"));
    public static final ResourceKey<Registry<Object>> ITEM_SUB_PREDICATE_TYPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("data_component_predicate_type"));
    public static final ResourceKey<Registry<Object>> DATA_COMPONENT_TYPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("data_component_type"));
    public static final ResourceKey<Registry<Object>> BLOCK_PREDICATE_TYPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("block_predicate_type"));
    public static final ResourceKey<Registry<Object>> LOOT_TABLE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("loot_table"));
    public static final ResourceKey<Registry<Object>> RECIPE =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("recipe"));
    public static final ResourceKey<Registry<Object>> ADVANCEMENT =
            ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("advancement"));

    private Registries() {}
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
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
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.advancements.Advancement;
import net.minecraft.advancements.predicates.entity.EntitySubPredicate;
import net.minecraft.advancements.triggers.CriterionTrigger;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.core.component.BlockTransformer;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.predicates.DataComponentPredicate;
import net.minecraft.core.particles.ParticleType;
import net.minecraft.gametest.framework.GameTestHelper;
import net.minecraft.gametest.framework.GameTestInstance;
import net.minecraft.gametest.framework.TestEnvironmentDefinition;
import net.minecraft.network.chat.ChatType;
import net.minecraft.network.chat.numbers.NumberFormatType;
import net.minecraft.server.dialog.Dialog;
import net.minecraft.server.dialog.action.Action;
import net.minecraft.server.dialog.body.DialogBody;
import net.minecraft.server.dialog.input.InputControl;
import net.minecraft.server.jsonrpc.IncomingRpcMethod;
import net.minecraft.server.jsonrpc.OutgoingRpcMethod;
import net.minecraft.server.level.TicketType;
import net.minecraft.server.permissions.Permission;
import net.minecraft.server.permissions.PermissionCheck;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.stats.StatType;
import net.minecraft.util.context.ContextKeySet;
import net.minecraft.util.debug.DebugSubscription;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.attribute.AttributeType;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.clock.WorldClock;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.entity.SulfurCubeArchetype;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.sensing.SensorType;
import net.minecraft.world.entity.ai.village.poi.PoiType;
import net.minecraft.world.entity.animal.chicken.ChickenSoundVariant;
import net.minecraft.world.entity.animal.chicken.ChickenVariant;
import net.minecraft.world.entity.animal.cow.CowSoundVariant;
import net.minecraft.world.entity.animal.cow.CowVariant;
import net.minecraft.world.entity.animal.feline.CatSoundVariant;
import net.minecraft.world.entity.animal.feline.CatVariant;
import net.minecraft.world.entity.animal.frog.FrogVariant;
import net.minecraft.world.entity.animal.nautilus.ZombieNautilusVariant;
import net.minecraft.world.entity.animal.pig.PigSoundVariant;
import net.minecraft.world.entity.animal.pig.PigVariant;
import net.minecraft.world.entity.animal.wolf.WolfSoundVariant;
import net.minecraft.world.entity.animal.wolf.WolfVariant;
import net.minecraft.world.entity.decoration.painting.PaintingVariant;
import net.minecraft.world.entity.npc.villager.VillagerProfession;
import net.minecraft.world.entity.npc.villager.VillagerType;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.entity.variant.SpawnCondition;
import net.minecraft.world.inventory.MenuType;
import net.minecraft.world.item.CreativeModeTab;
import net.minecraft.world.item.Instrument;
import net.minecraft.world.item.JukeboxSong;
import net.minecraft.world.item.alchemy.Potion;
import net.minecraft.world.item.consume_effects.ConsumeEffect;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.crafting.RecipeBookCategory;
import net.minecraft.world.item.crafting.RecipeSerializer;
import net.minecraft.world.item.crafting.RecipeType;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.item.crafting.display.SlotDisplay;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.item.enchantment.effects.EnchantmentEntityEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentLocationBasedEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentValueEffect;
import net.minecraft.world.item.enchantment.providers.EnchantmentProvider;
import net.minecraft.world.item.equipment.trim.TrimMaterial;
import net.minecraft.world.item.equipment.trim.TrimPattern;
import net.minecraft.world.item.slot.SlotSource;
import net.minecraft.world.item.trading.TradeSet;
import net.minecraft.world.item.trading.VillagerTrade;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.block.entity.BannerPattern;
import net.minecraft.world.level.block.entity.DecoratedPotPattern;
import net.minecraft.world.level.block.entity.trialspawner.TrialSpawnerConfig;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.dimension.DimensionType;
import net.minecraft.world.level.dimension.LevelStem;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.PositionSourceType;
import net.minecraft.world.level.gamerules.GameRule;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicateType;
import net.minecraft.world.level.levelgen.carver.WorldCarver;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.feature.Feature;
import net.minecraft.world.level.levelgen.feature.featuresize.FeatureSizeType;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacerType;
import net.minecraft.world.level.levelgen.feature.rootplacers.RootPlacerType;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.feature.treedecorators.TreeDecoratorType;
import net.minecraft.world.level.levelgen.feature.trunkplacers.TrunkPlacerType;
import net.minecraft.world.level.levelgen.flat.FlatLevelGeneratorPreset;
import net.minecraft.world.level.levelgen.heightproviders.HeightProviderType;
import net.minecraft.world.level.levelgen.material.condition.MaterialCondition;
import net.minecraft.world.level.levelgen.material.rule.MaterialRule;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.placement.PlacementModifier;
import net.minecraft.world.level.levelgen.presets.WorldPreset;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureSet;
import net.minecraft.world.level.levelgen.structure.StructureType;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;
import net.minecraft.world.level.levelgen.structure.pools.StructurePoolElementType;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
import net.minecraft.world.level.levelgen.structure.pools.alias.PoolAliasBinding;
import net.minecraft.world.level.levelgen.structure.templatesystem.PosRuleTestType;
import net.minecraft.world.level.levelgen.structure.templatesystem.RuleTestType;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorList;
import net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity.RuleBlockEntityModifierType;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
import net.minecraft.world.level.saveddata.maps.MapDecorationType;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.entries.LootPoolEntryContainer;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.nbt.NbtProvider;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
import net.minecraft.world.level.storage.loot.providers.score.ScoreboardNameProvider;
import net.minecraft.world.timeline.Timeline;

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
    public static final Identifier ROOT_REGISTRY_NAME = Identifier.withDefaultNamespace("root");
    public static final ResourceKey<Registry<Activity>> ACTIVITY = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("activity"));
    public static final ResourceKey<Registry<MapCodec<? extends BiomeSource>>> BIOME_SOURCE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/biome_source"));
    public static final ResourceKey<Registry<MapCodec<? extends BlockStateProvider>>> BLOCK_STATE_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/block_state_provider_type"));
    public static final ResourceKey<Registry<MapCodec<? extends WorldCarver>>> CARVER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/carver_type"));
    public static final ResourceKey<Registry<MapCodec<? extends ChunkGenerator>>> CHUNK_GENERATOR = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/chunk_generator"));
    public static final ResourceKey<Registry<ChunkStatus>> CHUNK_STATUS = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("chunk_status"));
    public static final ResourceKey<Registry<ArgumentTypeInfo<?, ?>>> COMMAND_ARGUMENT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("command_argument_type"));
    public static final ResourceKey<Registry<ConsumeEffect.Type<?>>> CONSUME_EFFECT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("consume_effect_type"));
    public static final ResourceKey<Registry<CreativeModeTab>> CREATIVE_MODE_TAB = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("creative_mode_tab"));
    public static final ResourceKey<Registry<Identifier>> CUSTOM_STAT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("custom_stat"));
    public static final ResourceKey<Registry<DataComponentPredicate.Type<?>>> DATA_COMPONENT_PREDICATE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("data_component_predicate_type"));
    public static final ResourceKey<Registry<GameRule<?>>> GAME_RULE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("game_rule"));
    public static final ResourceKey<Registry<DebugSubscription<?>>> DEBUG_SUBSCRIPTION = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("debug_subscription"));
    public static final ResourceKey<Registry<MapCodec<? extends DensityFunction>>> DENSITY_FUNCTION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/density_function_type"));
    public static final ResourceKey<Registry<MapCodec<? extends DialogBody>>> DIALOG_BODY_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("dialog_body_type"));
    public static final ResourceKey<Registry<MapCodec<? extends Dialog>>> DIALOG_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("dialog_type"));
    public static final ResourceKey<Registry<DataComponentType<?>>> ENCHANTMENT_EFFECT_COMPONENT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("enchantment_effect_component_type"));
    public static final ResourceKey<Registry<MapCodec<? extends EnchantmentEntityEffect>>> ENCHANTMENT_ENTITY_EFFECT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("enchantment_entity_effect_type"));
    public static final ResourceKey<Registry<MapCodec<? extends LevelBasedValue>>> ENCHANTMENT_LEVEL_BASED_VALUE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("enchantment_level_based_value_type"));
    public static final ResourceKey<Registry<MapCodec<? extends EnchantmentLocationBasedEffect>>> ENCHANTMENT_LOCATION_BASED_EFFECT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("enchantment_location_based_effect_type"));
    public static final ResourceKey<Registry<MapCodec<? extends EnchantmentProvider>>> ENCHANTMENT_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("enchantment_provider_type"));
    public static final ResourceKey<Registry<MapCodec<? extends EnchantmentValueEffect>>> ENCHANTMENT_VALUE_EFFECT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("enchantment_value_effect_type"));
    public static final ResourceKey<Registry<Codec<? extends EntitySubPredicate>>> ENTITY_SUB_PREDICATE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("entity_sub_predicate_type"));
    public static final ResourceKey<Registry<EnvironmentAttribute<?>>> ENVIRONMENT_ATTRIBUTE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("environment_attribute"));
    public static final ResourceKey<Registry<AttributeType<?>>> ATTRIBUTE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("attribute_type"));
    public static final ResourceKey<Registry<FeatureSizeType<?>>> FEATURE_SIZE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/feature_size_type"));
    public static final ResourceKey<Registry<MapCodec<? extends Feature>>> FEATURE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/feature_type"));
    public static final ResourceKey<Registry<MapCodec<? extends FloatProvider>>> FLOAT_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("float_provider_type"));
    public static final ResourceKey<Registry<FoliagePlacerType<?>>> FOLIAGE_PLACER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/foliage_placer_type"));
    public static final ResourceKey<Registry<GameEvent>> GAME_EVENT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("game_event"));
    public static final ResourceKey<Registry<HeightProviderType<?>>> HEIGHT_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("height_provider_type"));
    public static final ResourceKey<Registry<MapCodec<? extends InputControl>>> INPUT_CONTROL_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("input_control_type"));
    public static final ResourceKey<Registry<MapCodec<? extends IntProvider>>> INT_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("int_provider_type"));
    public static final ResourceKey<Registry<MapCodec<? extends SlotSource>>> SLOT_SOURCE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("slot_source_type"));
    public static final ResourceKey<Registry<MapCodec<? extends LootItemCondition>>> LOOT_CONDITION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("loot_condition_type"));
    public static final ResourceKey<Registry<MapCodec<? extends LootItemFunction>>> LOOT_FUNCTION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("loot_function_type"));
    public static final ResourceKey<Registry<MapCodec<? extends NbtProvider>>> LOOT_NBT_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("loot_nbt_provider_type"));
    public static final ResourceKey<Registry<MapCodec<? extends ContextFloatProvider>>> CONTEXT_FLOAT_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("context_float_provider_type"));
    public static final ResourceKey<Registry<MapCodec<? extends ContextIntProvider>>> CONTEXT_INT_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("context_int_provider_type"));
    public static final ResourceKey<Registry<MapCodec<? extends LootPoolEntryContainer>>> LOOT_POOL_ENTRY_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("loot_pool_entry_type"));
    public static final ResourceKey<Registry<MapCodec<? extends ScoreboardNameProvider>>> LOOT_SCORE_PROVIDER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("loot_score_provider_type"));
    public static final ResourceKey<Registry<MapDecorationType>> MAP_DECORATION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("map_decoration_type"));
    public static final ResourceKey<Registry<MapCodec<? extends MaterialCondition>>> MATERIAL_CONDITION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/material_condition_type"));
    public static final ResourceKey<Registry<MapCodec<? extends MaterialRule>>> MATERIAL_RULE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/material_rule_type"));
    public static final ResourceKey<Registry<MemoryModuleType<?>>> MEMORY_MODULE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("memory_module_type"));
    public static final ResourceKey<Registry<MenuType<?>>> MENU = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("menu"));
    public static final ResourceKey<Registry<NumberFormatType<?>>> NUMBER_FORMAT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("number_format_type"));
    public static final ResourceKey<Registry<MapCodec<? extends PlacementModifier>>> PLACEMENT_MODIFIER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/placement_modifier_type"));
    public static final ResourceKey<Registry<PoiType>> POINT_OF_INTEREST_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("point_of_interest_type"));
    public static final ResourceKey<Registry<MapCodec<? extends PoolAliasBinding>>> POOL_ALIAS_BINDING = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/pool_alias_binding"));
    public static final ResourceKey<Registry<PositionSourceType<?>>> POSITION_SOURCE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("position_source_type"));
    public static final ResourceKey<Registry<PosRuleTestType<?>>> POS_RULE_TEST = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("pos_rule_test"));
    public static final ResourceKey<Registry<Potion>> POTION = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("potion"));
    public static final ResourceKey<Registry<RecipeBookCategory>> RECIPE_BOOK_CATEGORY = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("recipe_book_category"));
    public static final ResourceKey<Registry<RecipeDisplay.Type<?>>> RECIPE_DISPLAY = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("recipe_display"));
    public static final ResourceKey<Registry<RecipeSerializer<?>>> RECIPE_SERIALIZER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("recipe_serializer"));
    public static final ResourceKey<Registry<RecipeType<?>>> RECIPE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("recipe_type"));
    public static final ResourceKey<Registry<RootPlacerType<?>>> ROOT_PLACER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/root_placer_type"));
    public static final ResourceKey<Registry<RuleBlockEntityModifierType<?>>> RULE_BLOCK_ENTITY_MODIFIER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("rule_block_entity_modifier"));
    public static final ResourceKey<Registry<RuleTestType<?>>> RULE_TEST = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("rule_test"));
    public static final ResourceKey<Registry<SensorType<?>>> SENSOR_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("sensor_type"));
    public static final ResourceKey<Registry<SlotDisplay.Type<?>>> SLOT_DISPLAY = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("slot_display"));
    public static final ResourceKey<Registry<MapCodec<? extends SpawnCondition>>> SPAWN_CONDITION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("spawn_condition_type"));
    public static final ResourceKey<Registry<StatType<?>>> STAT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("stat_type"));
    public static final ResourceKey<Registry<StructurePieceType>> STRUCTURE_PIECE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/structure_piece"));
    public static final ResourceKey<Registry<MapCodec<? extends StructurePlacement>>> STRUCTURE_PLACEMENT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/structure_placement"));
    public static final ResourceKey<Registry<StructurePoolElementType<?>>> STRUCTURE_POOL_ELEMENT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/structure_pool_element"));
    public static final ResourceKey<Registry<MapCodec<? extends StructureProcessor>>> STRUCTURE_PROCESSOR = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/structure_processor"));
    public static final ResourceKey<Registry<StructureType<?>>> STRUCTURE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/structure_type"));
    public static final ResourceKey<Registry<MapCodec<? extends Action>>> DIALOG_ACTION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("dialog_action_type"));
    public static final ResourceKey<Registry<MapCodec<? extends TestEnvironmentDefinition<?>>>> TEST_ENVIRONMENT_DEFINITION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("test_environment_definition_type"));
    public static final ResourceKey<Registry<Consumer<GameTestHelper>>> TEST_FUNCTION = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("test_function"));
    public static final ResourceKey<Registry<MapCodec<? extends GameTestInstance>>> TEST_INSTANCE_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("test_instance_type"));
    public static final ResourceKey<Registry<TicketType>> TICKET_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("ticket_type"));
    public static final ResourceKey<Registry<TreeDecoratorType<?>>> TREE_DECORATOR_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/tree_decorator_type"));
    public static final ResourceKey<Registry<TrunkPlacerType<?>>> TRUNK_PLACER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/trunk_placer_type"));
    public static final ResourceKey<Registry<VillagerProfession>> VILLAGER_PROFESSION = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("villager_profession"));
    public static final ResourceKey<Registry<VillagerType>> VILLAGER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("villager_type"));
    public static final ResourceKey<Registry<IncomingRpcMethod<?, ?>>> INCOMING_RPC_METHOD = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("incoming_rpc_methods"));
    public static final ResourceKey<Registry<OutgoingRpcMethod<?, ?>>> OUTGOING_RPC_METHOD = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("outgoing_rpc_methods"));
    public static final ResourceKey<Registry<MapCodec<? extends Permission>>> PERMISSION_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("permission_type"));
    public static final ResourceKey<Registry<MapCodec<? extends PermissionCheck>>> PERMISSION_CHECK_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("permission_check_type"));
    public static final ResourceKey<Registry<ContextKeySet>> CONTEXT_KEY_SET = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("context_key_set"));
    public static final ResourceKey<Registry<BannerPattern>> BANNER_PATTERN = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("banner_pattern"));
    public static final ResourceKey<Registry<BlockStateProvider>> BLOCK_STATE_PROVIDER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/block_state_provider"));
    public static final ResourceKey<Registry<CatSoundVariant>> CAT_SOUND_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("cat_sound_variant"));
    public static final ResourceKey<Registry<CatVariant>> CAT_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("cat_variant"));
    public static final ResourceKey<Registry<WorldCarver>> CARVER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/carver"));
    public static final ResourceKey<Registry<ChatType>> CHAT_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("chat_type"));
    public static final ResourceKey<Registry<ChickenSoundVariant>> CHICKEN_SOUND_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("chicken_sound_variant"));
    public static final ResourceKey<Registry<ChickenVariant>> CHICKEN_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("chicken_variant"));
    public static final ResourceKey<Registry<ZombieNautilusVariant>> ZOMBIE_NAUTILUS_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("zombie_nautilus_variant"));
    public static final ResourceKey<Registry<CowSoundVariant>> COW_SOUND_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("cow_sound_variant"));
    public static final ResourceKey<Registry<CowVariant>> COW_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("cow_variant"));
    public static final ResourceKey<Registry<DensityFunction>> DENSITY_FUNCTION = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/density_function"));
    public static final ResourceKey<Registry<Dialog>> DIALOG = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("dialog"));
    public static final ResourceKey<Registry<EnchantmentProvider>> ENCHANTMENT_PROVIDER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("enchantment_provider"));
    public static final ResourceKey<Registry<FlatLevelGeneratorPreset>> FLAT_LEVEL_GENERATOR_PRESET = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/flat_level_generator_preset"));
    public static final ResourceKey<Registry<FrogVariant>> FROG_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("frog_variant"));
    public static final ResourceKey<Registry<Instrument>> INSTRUMENT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("instrument"));
    public static final ResourceKey<Registry<JukeboxSong>> JUKEBOX_SONG = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("jukebox_song"));
    public static final ResourceKey<Registry<MaterialCondition>> MATERIAL_CONDITION = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/material_condition"));
    public static final ResourceKey<Registry<MaterialRule>> MATERIAL_RULE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/material_rule"));
    public static final ResourceKey<Registry<MultiNoiseBiomeSourceParameterList>> MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/multi_noise_biome_source_parameter_list"));
    public static final ResourceKey<Registry<NormalNoise>> NOISE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/noise"));
    public static final ResourceKey<Registry<PaintingVariant>> PAINTING_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("painting_variant"));
    public static final ResourceKey<Registry<PigSoundVariant>> PIG_SOUND_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("pig_sound_variant"));
    public static final ResourceKey<Registry<PigVariant>> PIG_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("pig_variant"));
    public static final ResourceKey<Registry<SulfurCubeArchetype>> SULFUR_CUBE_ARCHETYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("sulfur_cube_archetype"));
    public static final ResourceKey<Registry<TestEnvironmentDefinition<?>>> TEST_ENVIRONMENT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("test_environment"));
    public static final ResourceKey<Registry<GameTestInstance>> TEST_INSTANCE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("test_instance"));
    public static final ResourceKey<Registry<Timeline>> TIMELINE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("timeline"));
    public static final ResourceKey<Registry<TradeSet>> TRADE_SET = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("trade_set"));
    public static final ResourceKey<Registry<TrialSpawnerConfig>> TRIAL_SPAWNER_CONFIG = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("trial_spawner"));
    public static final ResourceKey<Registry<CriterionTrigger<?>>> TRIGGER_TYPE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("trigger_type"));
    public static final ResourceKey<Registry<TrimMaterial>> TRIM_MATERIAL = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("trim_material"));
    public static final ResourceKey<Registry<TrimPattern>> TRIM_PATTERN = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("trim_pattern"));
    public static final ResourceKey<Registry<VillagerTrade>> VILLAGER_TRADE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("villager_trade"));
    public static final ResourceKey<Registry<WolfVariant>> WOLF_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("wolf_variant"));
    public static final ResourceKey<Registry<WolfSoundVariant>> WOLF_SOUND_VARIANT = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("wolf_sound_variant"));
    public static final ResourceKey<Registry<WorldClock>> WORLD_CLOCK = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("world_clock"));
    public static final ResourceKey<Registry<WorldPreset>> WORLD_PRESET = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("worldgen/world_preset"));
    public static final ResourceKey<Registry<DecoratedPotPattern>> DECORATED_POT_PATTERN = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("decorated_pot_pattern"));
    public static final ResourceKey<Registry<BlockTransformer>> BLOCK_TRANSFORMER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("block_transformer"));
    public static final ResourceKey<Registry<LevelStem>> LEVEL_STEM = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("dimension"));
    public static final ResourceKey<Registry<LootItemFunction>> ITEM_MODIFIER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("item_modifier"));
    public static final ResourceKey<Registry<LootItemCondition>> PREDICATE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("predicate"));
    public static final ResourceKey<Registry<SlotSource>> SLOT_SOURCE = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("slot_source"));
    public static final ResourceKey<Registry<ContextFloatProvider>> CONTEXT_FLOAT_PROVIDER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("context_float_provider"));
    public static final ResourceKey<Registry<ContextIntProvider>> CONTEXT_INT_PROVIDER = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("context_int_provider"));
    public static ResourceKey<Level> levelStemToLevel(ResourceKey<LevelStem> levelStem) {
        return ResourceKey.create(DIMENSION, levelStem.identifier());
    }
    public static ResourceKey<LevelStem> levelToLevelStem(ResourceKey<Level> level) {
        return ResourceKey.create(LEVEL_STEM, level.identifier());
    }
    public static String elementsDirPath(ResourceKey<? extends Registry<?>> registryKey) {
        return registryKey.identifier().getPath();
    }
    public static String tagsDirPath(ResourceKey<? extends Registry<?>> registryKey) {
        return "tags/" + registryKey.identifier().getPath();
    }
    public static String componentsDirPath(ResourceKey<? extends Registry<?>> registryKey) {
        return "components/" + registryKey.identifier().getPath();
    }
}

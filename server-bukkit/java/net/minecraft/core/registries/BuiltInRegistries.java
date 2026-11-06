// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.registries;

import net.minecraft.core.DefaultedMappedRegistry;
import net.minecraft.core.DefaultedRegistry;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.Fluids;
import org.basaltmc.bukkit.facade.RegistryTags;
import org.basaltmc.bukkit.Registries;
import org.basaltmc.bukkit.WorldNatives;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.advancements.predicates.entity.EntitySubPredicate;
import net.minecraft.advancements.triggers.CriterionTrigger;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.component.DataComponentInitializers;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.predicates.DataComponentPredicate;
import net.minecraft.core.particles.ParticleType;
import net.minecraft.gametest.framework.GameTestHelper;
import net.minecraft.gametest.framework.GameTestInstance;
import net.minecraft.gametest.framework.TestEnvironmentDefinition;
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
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.sensing.SensorType;
import net.minecraft.world.entity.ai.village.poi.PoiType;
import net.minecraft.world.entity.npc.villager.VillagerProfession;
import net.minecraft.world.entity.npc.villager.VillagerType;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.entity.variant.SpawnCondition;
import net.minecraft.world.inventory.MenuType;
import net.minecraft.world.item.CreativeModeTab;
import net.minecraft.world.item.alchemy.Potion;
import net.minecraft.world.item.consume_effects.ConsumeEffect;
import net.minecraft.world.item.crafting.RecipeBookCategory;
import net.minecraft.world.item.crafting.RecipeSerializer;
import net.minecraft.world.item.crafting.RecipeType;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.item.crafting.display.SlotDisplay;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.item.enchantment.effects.EnchantmentEntityEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentLocationBasedEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentValueEffect;
import net.minecraft.world.item.enchantment.providers.EnchantmentProvider;
import net.minecraft.world.item.slot.SlotSource;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.PositionSourceType;
import net.minecraft.world.level.gamerules.GameRule;
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
import net.minecraft.world.level.levelgen.heightproviders.HeightProviderType;
import net.minecraft.world.level.levelgen.material.condition.MaterialCondition;
import net.minecraft.world.level.levelgen.material.rule.MaterialRule;
import net.minecraft.world.level.levelgen.placement.PlacementModifier;
import net.minecraft.world.level.levelgen.structure.StructureType;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;
import net.minecraft.world.level.levelgen.structure.pools.StructurePoolElementType;
import net.minecraft.world.level.levelgen.structure.pools.alias.PoolAliasBinding;
import net.minecraft.world.level.levelgen.structure.templatesystem.PosRuleTestType;
import net.minecraft.world.level.levelgen.structure.templatesystem.RuleTestType;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity.RuleBlockEntityModifierType;
import net.minecraft.world.level.saveddata.maps.MapDecorationType;
import net.minecraft.world.level.storage.loot.entries.LootPoolEntryContainer;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.nbt.NbtProvider;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
import net.minecraft.world.level.storage.loot.providers.score.ScoreboardNameProvider;

public class BuiltInRegistries {
    public static final DefaultedRegistry<Block> BLOCK = blocks();
    public static final DefaultedRegistry<Item> ITEM = items();
    public static final DefaultedRegistry<Fluid> FLUID = fluids();
    public static final DefaultedRegistry<EntityType<?>> ENTITY_TYPE = entityTypes();
    public static final Registry<BlockEntityType<?>> BLOCK_ENTITY_TYPE =
        new MappedRegistry<>(net.minecraft.core.registries.Registries.BLOCK_ENTITY_TYPE);

    private BuiltInRegistries() {}

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> Registry<T> empty(net.minecraft.resources.ResourceKey<?> key) {
        return new MappedRegistry<T>((net.minecraft.resources.ResourceKey) key);
    }

    private static Registry<SoundEvent> sounds() {
        MappedRegistry<SoundEvent> registry = (MappedRegistry<SoundEvent>) (MappedRegistry<?>) BuiltInRegistries.<SoundEvent>empty(net.minecraft.core.registries.Registries.SOUND_EVENT);
        try (java.io.InputStream in = BuiltInRegistries.class.getResourceAsStream("/sound_events.txt")) {
            if (in != null) {
                for (String line : new String(in.readAllBytes(), java.nio.charset.StandardCharsets.UTF_8).split("\\R")) {
                    String id = line.trim();
                    SoundEvent event = id.isEmpty() ? null : net.minecraft.sounds.SoundEvents.byId(id);
                    if (event != null) {
                        registry.register(event.location(), event);
                    }
                }
            }
        } catch (java.io.IOException e) {
            throw new java.io.UncheckedIOException(e);
        }
        return registry;
    }

    private static DefaultedRegistry<Block> blocks() {
        DefaultedMappedRegistry<Block> registry =
            new DefaultedMappedRegistry<>("minecraft:air", net.minecraft.core.registries.Registries.BLOCK);
        for (int i = 0; i < Block.blockCount(); i++) {
            Block block = Block.byIndex(i);
            registry.register(block.blockKey(), block);
        }
        registry.tagsFrom(() -> RegistryTags.bind(registry, "blocks"));
        return registry;
    }

    private static DefaultedRegistry<Item> items() {
        DefaultedMappedRegistry<Item> registry =
            new DefaultedMappedRegistry<>("minecraft:air", net.minecraft.core.registries.Registries.ITEM);
        for (String id : Registries.itemIds()) {
            Identifier key = Identifier.parse(id);
            registry.register(key, net.minecraft.world.item.ItemClasses.create(key));
        }
        registry.tagsFrom(() -> RegistryTags.bind(registry, "items"));
        return registry;
    }

    private static DefaultedMappedRegistry<GameEvent> gameEvents() {
        DefaultedMappedRegistry<GameEvent> registry =
            new DefaultedMappedRegistry<>("minecraft:step", net.minecraft.core.registries.Registries.GAME_EVENT);
        registry.tagsFrom(() -> org.basaltmc.bukkit.facade.GameEventTagData.bind(registry));
        return registry;
    }

    private static DefaultedRegistry<Fluid> fluids() {
        DefaultedMappedRegistry<Fluid> registry =
            new DefaultedMappedRegistry<>("minecraft:empty", net.minecraft.core.registries.Registries.FLUID);
        registry.register(Identifier.withDefaultNamespace("empty"), Fluids.EMPTY);
        registry.register(Identifier.withDefaultNamespace("flowing_water"), Fluids.FLOWING_WATER);
        registry.register(Identifier.withDefaultNamespace("water"), Fluids.WATER);
        registry.register(Identifier.withDefaultNamespace("flowing_lava"), Fluids.FLOWING_LAVA);
        registry.register(Identifier.withDefaultNamespace("lava"), Fluids.LAVA);
        return registry;
    }

    private static DefaultedRegistry<EntityType<?>> entityTypes() {
        DefaultedMappedRegistry<EntityType<?>> registry =
            new DefaultedMappedRegistry<>("minecraft:pig", net.minecraft.core.registries.Registries.ENTITY_TYPE);
        for (String id : WorldNatives.entityTypes()) {
            Identifier key = Identifier.parse(id);
            registry.register(key, new EntityType<>(key));
        }
        registry.tagsFrom(() -> RegistryTags.bind(registry, "entity_types"));
        return registry;
    }

    public static final DataComponentInitializers DATA_COMPONENT_INITIALIZERS = null;
    public static final DefaultedMappedRegistry<GameEvent> GAME_EVENT = gameEvents();
    public static final Registry<SoundEvent> SOUND_EVENT = sounds();
    public static final Registry<MobEffect> MOB_EFFECT = net.minecraft.world.effect.MobEffects.registry();
    public static final Registry<DebugSubscription<?>> DEBUG_SUBSCRIPTION = new MappedRegistry<>(net.minecraft.core.registries.Registries.DEBUG_SUBSCRIPTION);
    public static final Registry<Potion> POTION = new MappedRegistry<>(net.minecraft.core.registries.Registries.POTION);
    public static final Registry<ParticleType<?>> PARTICLE_TYPE = empty(net.minecraft.core.registries.Registries.PARTICLE_TYPE);
    public static final Registry<Identifier> CUSTOM_STAT = new MappedRegistry<>(net.minecraft.core.registries.Registries.CUSTOM_STAT);
    public static final DefaultedRegistry<ChunkStatus> CHUNK_STATUS = new DefaultedMappedRegistry<>("minecraft:empty", net.minecraft.core.registries.Registries.CHUNK_STATUS);
    public static final Registry<RuleTestType<?>> RULE_TEST = new MappedRegistry<>(net.minecraft.core.registries.Registries.RULE_TEST);
    public static final Registry<RuleBlockEntityModifierType<?>> RULE_BLOCK_ENTITY_MODIFIER = new MappedRegistry<>(net.minecraft.core.registries.Registries.RULE_BLOCK_ENTITY_MODIFIER);
    public static final Registry<PosRuleTestType<?>> POS_RULE_TEST = new MappedRegistry<>(net.minecraft.core.registries.Registries.POS_RULE_TEST);
    public static final Registry<MenuType<?>> MENU = new MappedRegistry<>(net.minecraft.core.registries.Registries.MENU);
    public static final Registry<RecipeType<?>> RECIPE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.RECIPE_TYPE);
    public static final Registry<RecipeSerializer<?>> RECIPE_SERIALIZER = new MappedRegistry<>(net.minecraft.core.registries.Registries.RECIPE_SERIALIZER);
    public static final Registry<Attribute> ATTRIBUTE = net.minecraft.world.entity.ai.attributes.Attributes.registry();
    public static final Registry<PositionSourceType<?>> POSITION_SOURCE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.POSITION_SOURCE_TYPE);
    public static final Registry<ArgumentTypeInfo<?, ?>> COMMAND_ARGUMENT_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.COMMAND_ARGUMENT_TYPE);
    public static final Registry<StatType<?>> STAT_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.STAT_TYPE);
    public static final DefaultedRegistry<VillagerType> VILLAGER_TYPE = new DefaultedMappedRegistry<>("minecraft:plains", net.minecraft.core.registries.Registries.VILLAGER_TYPE);
    public static final DefaultedRegistry<VillagerProfession> VILLAGER_PROFESSION = new DefaultedMappedRegistry<>("minecraft:none", net.minecraft.core.registries.Registries.VILLAGER_PROFESSION);
    public static final Registry<PoiType> POINT_OF_INTEREST_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE);
    public static final DefaultedRegistry<MemoryModuleType<?>> MEMORY_MODULE_TYPE = net.minecraft.world.entity.ai.memory.MemoryModuleType.registry();
    public static final DefaultedRegistry<SensorType<?>> SENSOR_TYPE = new DefaultedMappedRegistry<>("minecraft:dummy", net.minecraft.core.registries.Registries.SENSOR_TYPE);
    public static final Registry<Activity> ACTIVITY = net.minecraft.world.entity.schedule.Activity.registry();
    public static final Registry<MapCodec<? extends LootPoolEntryContainer>> LOOT_POOL_ENTRY_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.LOOT_POOL_ENTRY_TYPE);
    public static final Registry<MapCodec<? extends LootItemFunction>> LOOT_FUNCTION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.LOOT_FUNCTION_TYPE);
    public static final Registry<MapCodec<? extends LootItemCondition>> LOOT_CONDITION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.LOOT_CONDITION_TYPE);
    public static final Registry<MapCodec<? extends ContextFloatProvider>> CONTEXT_FLOAT_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.CONTEXT_FLOAT_PROVIDER_TYPE);
    public static final Registry<MapCodec<? extends ContextIntProvider>> CONTEXT_INT_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.CONTEXT_INT_PROVIDER_TYPE);
    public static final Registry<MapCodec<? extends NbtProvider>> LOOT_NBT_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.LOOT_NBT_PROVIDER_TYPE);
    public static final Registry<MapCodec<? extends ScoreboardNameProvider>> LOOT_SCORE_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.LOOT_SCORE_PROVIDER_TYPE);
    public static final Registry<MapCodec<? extends FloatProvider>> FLOAT_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.FLOAT_PROVIDER_TYPE);
    public static final Registry<MapCodec<? extends IntProvider>> INT_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.INT_PROVIDER_TYPE);
    public static final Registry<HeightProviderType<?>> HEIGHT_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.HEIGHT_PROVIDER_TYPE);
    public static final Registry<BlockPredicateType<?>> BLOCK_PREDICATE_TYPE = empty(net.minecraft.core.registries.Registries.BLOCK_PREDICATE_TYPE);
    public static final Registry<MapCodec<? extends WorldCarver>> CARVER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.CARVER_TYPE);
    public static final Registry<MapCodec<? extends Feature>> FEATURE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.FEATURE_TYPE);
    public static final Registry<MapCodec<? extends StructurePlacement>> STRUCTURE_PLACEMENT = new MappedRegistry<>(net.minecraft.core.registries.Registries.STRUCTURE_PLACEMENT);
    public static final Registry<StructurePieceType> STRUCTURE_PIECE = new MappedRegistry<>(net.minecraft.core.registries.Registries.STRUCTURE_PIECE);
    public static final Registry<StructureType<?>> STRUCTURE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.STRUCTURE_TYPE);
    public static final Registry<MapCodec<? extends PlacementModifier>> PLACEMENT_MODIFIER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.PLACEMENT_MODIFIER_TYPE);
    public static final Registry<MapCodec<? extends BlockStateProvider>> BLOCK_STATE_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.BLOCK_STATE_PROVIDER_TYPE);
    public static final Registry<FoliagePlacerType<?>> FOLIAGE_PLACER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.FOLIAGE_PLACER_TYPE);
    public static final Registry<TrunkPlacerType<?>> TRUNK_PLACER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.TRUNK_PLACER_TYPE);
    public static final Registry<RootPlacerType<?>> ROOT_PLACER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ROOT_PLACER_TYPE);
    public static final Registry<TreeDecoratorType<?>> TREE_DECORATOR_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.TREE_DECORATOR_TYPE);
    public static final Registry<FeatureSizeType<?>> FEATURE_SIZE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.FEATURE_SIZE_TYPE);
    public static final Registry<MapCodec<? extends BiomeSource>> BIOME_SOURCE = new MappedRegistry<>(net.minecraft.core.registries.Registries.BIOME_SOURCE);
    public static final Registry<MapCodec<? extends ChunkGenerator>> CHUNK_GENERATOR = new MappedRegistry<>(net.minecraft.core.registries.Registries.CHUNK_GENERATOR);
    public static final Registry<MapCodec<? extends MaterialCondition>> MATERIAL_CONDITION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.MATERIAL_CONDITION_TYPE);
    public static final Registry<MapCodec<? extends MaterialRule>> MATERIAL_RULE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.MATERIAL_RULE_TYPE);
    public static final Registry<MapCodec<? extends DensityFunction>> DENSITY_FUNCTION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.DENSITY_FUNCTION_TYPE);
    public static final Registry<MapCodec<? extends StructureProcessor>> STRUCTURE_PROCESSOR = new MappedRegistry<>(net.minecraft.core.registries.Registries.STRUCTURE_PROCESSOR);
    public static final Registry<StructurePoolElementType<?>> STRUCTURE_POOL_ELEMENT = new MappedRegistry<>(net.minecraft.core.registries.Registries.STRUCTURE_POOL_ELEMENT);
    public static final Registry<MapCodec<? extends PoolAliasBinding>> POOL_ALIAS_BINDING_TYPE = null;
    public static final Registry<CreativeModeTab> CREATIVE_MODE_TAB = new MappedRegistry<>(net.minecraft.core.registries.Registries.CREATIVE_MODE_TAB);
    public static final Registry<CriterionTrigger<?>> TRIGGER_TYPES = null;
    public static final Registry<NumberFormatType<?>> NUMBER_FORMAT_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.NUMBER_FORMAT_TYPE);
    public static final Registry<DataComponentType<?>> DATA_COMPONENT_TYPE = net.minecraft.core.component.DataComponents.registry();
    public static final Registry<GameRule<?>> GAME_RULE = new MappedRegistry<>(net.minecraft.core.registries.Registries.GAME_RULE);
    public static final Registry<Codec<? extends EntitySubPredicate>> ENTITY_SUB_PREDICATE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ENTITY_SUB_PREDICATE_TYPE);
    public static final Registry<DataComponentPredicate.Type<?>> DATA_COMPONENT_PREDICATE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.DATA_COMPONENT_PREDICATE_TYPE);
    public static final Registry<MapDecorationType> MAP_DECORATION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.MAP_DECORATION_TYPE);
    public static final Registry<DataComponentType<?>> ENCHANTMENT_EFFECT_COMPONENT_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ENCHANTMENT_EFFECT_COMPONENT_TYPE);
    public static final Registry<MapCodec<? extends LevelBasedValue>> ENCHANTMENT_LEVEL_BASED_VALUE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ENCHANTMENT_LEVEL_BASED_VALUE_TYPE);
    public static final Registry<MapCodec<? extends EnchantmentEntityEffect>> ENCHANTMENT_ENTITY_EFFECT_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ENCHANTMENT_ENTITY_EFFECT_TYPE);
    public static final Registry<MapCodec<? extends EnchantmentLocationBasedEffect>> ENCHANTMENT_LOCATION_BASED_EFFECT_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ENCHANTMENT_LOCATION_BASED_EFFECT_TYPE);
    public static final Registry<MapCodec<? extends EnchantmentValueEffect>> ENCHANTMENT_VALUE_EFFECT_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ENCHANTMENT_VALUE_EFFECT_TYPE);
    public static final Registry<MapCodec<? extends EnchantmentProvider>> ENCHANTMENT_PROVIDER_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ENCHANTMENT_PROVIDER_TYPE);
    public static final Registry<ConsumeEffect.Type<?>> CONSUME_EFFECT_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.CONSUME_EFFECT_TYPE);
    public static final Registry<RecipeDisplay.Type<?>> RECIPE_DISPLAY = new MappedRegistry<>(net.minecraft.core.registries.Registries.RECIPE_DISPLAY);
    public static final Registry<SlotDisplay.Type<?>> SLOT_DISPLAY = new MappedRegistry<>(net.minecraft.core.registries.Registries.SLOT_DISPLAY);
    public static final Registry<RecipeBookCategory> RECIPE_BOOK_CATEGORY = new MappedRegistry<>(net.minecraft.core.registries.Registries.RECIPE_BOOK_CATEGORY);
    public static final Registry<TicketType> TICKET_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.TICKET_TYPE);
    public static final Registry<IncomingRpcMethod<?, ?>> INCOMING_RPC_METHOD = new MappedRegistry<>(net.minecraft.core.registries.Registries.INCOMING_RPC_METHOD);
    public static final Registry<OutgoingRpcMethod<?, ?>> OUTGOING_RPC_METHOD = new MappedRegistry<>(net.minecraft.core.registries.Registries.OUTGOING_RPC_METHOD);
    public static final Registry<MapCodec<? extends TestEnvironmentDefinition<?>>> TEST_ENVIRONMENT_DEFINITION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.TEST_ENVIRONMENT_DEFINITION_TYPE);
    public static final Registry<MapCodec<? extends GameTestInstance>> TEST_INSTANCE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.TEST_INSTANCE_TYPE);
    public static final Registry<MapCodec<? extends SpawnCondition>> SPAWN_CONDITION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.SPAWN_CONDITION_TYPE);
    public static final Registry<MapCodec<? extends Dialog>> DIALOG_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.DIALOG_TYPE);
    public static final Registry<MapCodec<? extends Action>> DIALOG_ACTION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.DIALOG_ACTION_TYPE);
    public static final Registry<MapCodec<? extends InputControl>> INPUT_CONTROL_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.INPUT_CONTROL_TYPE);
    public static final Registry<MapCodec<? extends DialogBody>> DIALOG_BODY_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.DIALOG_BODY_TYPE);
    public static final Registry<MapCodec<? extends Permission>> PERMISSION_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.PERMISSION_TYPE);
    public static final Registry<MapCodec<? extends PermissionCheck>> PERMISSION_CHECK_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.PERMISSION_CHECK_TYPE);
    public static final Registry<EnvironmentAttribute<?>> ENVIRONMENT_ATTRIBUTE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ENVIRONMENT_ATTRIBUTE);
    public static final Registry<AttributeType<?>> ATTRIBUTE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.ATTRIBUTE_TYPE);
    public static final Registry<MapCodec<? extends SlotSource>> SLOT_SOURCE_TYPE = new MappedRegistry<>(net.minecraft.core.registries.Registries.SLOT_SOURCE_TYPE);
    public static final Registry<ContextKeySet> CONTEXT_KEY_SET = new MappedRegistry<>(net.minecraft.core.registries.Registries.CONTEXT_KEY_SET);
    public static final Registry<Consumer<GameTestHelper>> TEST_FUNCTION = new MappedRegistry<>(net.minecraft.core.registries.Registries.TEST_FUNCTION);
    public static final Registry<? extends Registry<?>> REGISTRY = null;
    public static void bootStrap() {
        return;
    }
    public static <T> HolderGetter<T> acquireBootstrapRegistrationLookup(Registry<T> registry) {
        return ((MappedRegistry<T>) registry).createRegistrationLookup();
    }
    interface RegistryBootstrap<T> {
        Object run(Registry<T> registry);
    }
}

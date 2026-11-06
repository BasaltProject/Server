// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.Registry;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.EncoderCache;
import net.minecraft.util.Unit;
import net.minecraft.world.LockCode;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.animal.axolotl.Axolotl;
import net.minecraft.world.entity.animal.chicken.ChickenSoundVariant;
import net.minecraft.world.entity.animal.chicken.ChickenVariant;
import net.minecraft.world.entity.animal.cow.CowSoundVariant;
import net.minecraft.world.entity.animal.cow.CowVariant;
import net.minecraft.world.entity.animal.cow.MushroomCow;
import net.minecraft.world.entity.animal.equine.Llama;
import net.minecraft.world.entity.animal.equine.Variant;
import net.minecraft.world.entity.animal.feline.CatSoundVariant;
import net.minecraft.world.entity.animal.feline.CatVariant;
import net.minecraft.world.entity.animal.fish.Salmon;
import net.minecraft.world.entity.animal.fish.TropicalFish;
import net.minecraft.world.entity.animal.fox.Fox;
import net.minecraft.world.entity.animal.frog.FrogVariant;
import net.minecraft.world.entity.animal.nautilus.ZombieNautilusVariant;
import net.minecraft.world.entity.animal.parrot.Parrot;
import net.minecraft.world.entity.animal.pig.PigSoundVariant;
import net.minecraft.world.entity.animal.pig.PigVariant;
import net.minecraft.world.entity.animal.rabbit.Rabbit;
import net.minecraft.world.entity.animal.wolf.WolfSoundVariant;
import net.minecraft.world.entity.animal.wolf.WolfVariant;
import net.minecraft.world.entity.decoration.painting.PaintingVariant;
import net.minecraft.world.entity.npc.villager.VillagerType;
import net.minecraft.world.food.FoodProperties;
import net.minecraft.world.food.VillagerFood;
import net.minecraft.world.item.AdventureModePredicate;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.JukeboxPlayable;
import net.minecraft.world.item.Rarity;
import net.minecraft.world.item.alchemy.PotionContents;
import net.minecraft.world.item.component.AttackRange;
import net.minecraft.world.item.component.Bees;
import net.minecraft.world.item.component.BlockItemStateProperties;
import net.minecraft.world.item.component.BlocksAttacks;
import net.minecraft.world.item.component.BrewingFuel;
import net.minecraft.world.item.component.BundleContents;
import net.minecraft.world.item.component.ChargedProjectiles;
import net.minecraft.world.item.component.Compostable;
import net.minecraft.world.item.component.Consumable;
import net.minecraft.world.item.component.CookingFuel;
import net.minecraft.world.item.component.CustomData;
import net.minecraft.world.item.component.CustomModelData;
import net.minecraft.world.item.component.DamageResistant;
import net.minecraft.world.item.component.DeathProtection;
import net.minecraft.world.item.component.DebugStickState;
import net.minecraft.world.item.component.DyedItemColor;
import net.minecraft.world.item.component.FireworkExplosion;
import net.minecraft.world.item.component.Fireworks;
import net.minecraft.world.item.component.InstrumentComponent;
import net.minecraft.world.item.component.ItemAttributeModifiers;
import net.minecraft.world.item.component.ItemContainerContents;
import net.minecraft.world.item.component.ItemLore;
import net.minecraft.world.item.component.KineticWeapon;
import net.minecraft.world.item.component.LodestoneTracker;
import net.minecraft.world.item.component.MapDecorations;
import net.minecraft.world.item.component.MapPostProcessing;
import net.minecraft.world.item.component.MobVisibility;
import net.minecraft.world.item.component.OminousBottleAmplifier;
import net.minecraft.world.item.component.PiercingWeapon;
import net.minecraft.world.item.component.ResolvableProfile;
import net.minecraft.world.item.component.SeededContainerLoot;
import net.minecraft.world.item.component.SulfurCubeContent;
import net.minecraft.world.item.component.SuspiciousStewEffects;
import net.minecraft.world.item.component.SwingAnimation;
import net.minecraft.world.item.component.Tool;
import net.minecraft.world.item.component.TooltipDisplay;
import net.minecraft.world.item.component.TypedEntityData;
import net.minecraft.world.item.component.UseCooldown;
import net.minecraft.world.item.component.UseEffects;
import net.minecraft.world.item.component.UseRemainder;
import net.minecraft.world.item.component.Weapon;
import net.minecraft.world.item.component.WritableBookContent;
import net.minecraft.world.item.component.WrittenBookContent;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.enchantment.Enchantable;
import net.minecraft.world.item.enchantment.ItemEnchantments;
import net.minecraft.world.item.enchantment.Repairable;
import net.minecraft.world.item.equipment.Equippable;
import net.minecraft.world.item.equipment.trim.ArmorTrim;
import net.minecraft.world.item.equipment.trim.TrimMaterial;
import net.minecraft.world.level.block.entity.BannerPattern;
import net.minecraft.world.level.block.entity.BannerPatternLayers;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.entity.DecoratedPotPattern;
import net.minecraft.world.level.block.entity.PotDecorations;
import net.minecraft.world.level.block.entity.SignText;
import net.minecraft.world.level.saveddata.maps.MapId;
public class DataComponents {
    private static final net.minecraft.core.MappedRegistry<DataComponentType<?>> REGISTRY = newRegistry();

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static net.minecraft.core.MappedRegistry<DataComponentType<?>> newRegistry() {
        return new net.minecraft.core.MappedRegistry(net.minecraft.core.registries.Registries.DATA_COMPONENT_TYPE);
    }

    public static Registry<DataComponentType<?>> registry() {
        return REGISTRY;
    }

    private static <T> DataComponentType<T> add(String name, DataComponentType<T> type) {
        REGISTRY.register(Identifier.withDefaultNamespace(name), type);
        return type;
    }

    private static <T> DataComponentType.Builder<T> networked(String name, DataComponentType.Builder<T> builder) {
        return org.basaltmc.bukkit.facade.StreamSpecs.hasComponent(name)
            ? builder.networkSynchronized(org.basaltmc.bukkit.facade.StreamSpecs.<T>component(name)) : builder;
    }

    private static <T> DataComponentType<T> type(String name, Codec<T> codec) {
        return add(name, networked(name, DataComponentType.<T>builder().persistent(codec)).build(name));
    }

    private static <T> DataComponentType<T> type(String name) {
        return add(name, networked(name, DataComponentType.<T>builder()).build(name));
    }

    private static <T> DataComponentType<T> wire(String name, Codec<T> codec, net.minecraft.network.codec.StreamCodec<? super net.minecraft.network.RegistryFriendlyByteBuf, T> stream) {
        DataComponentType.Builder<T> builder = DataComponentType.<T>builder().networkSynchronized(stream);
        return add(name, (codec == null ? builder : builder.persistent(codec)).build(name));
    }

    static final EncoderCache ENCODER_CACHE = null;
    public static final DataComponentType<CustomData> CUSTOM_DATA = type("custom_data");
    public static final DataComponentType<Integer> MAX_STACK_SIZE = wire("max_stack_size", Codec.INT, net.minecraft.network.codec.ByteBufCodecs.VAR_INT);
    public static final DataComponentType<Integer> MAX_DAMAGE = wire("max_damage", Codec.INT, net.minecraft.network.codec.ByteBufCodecs.VAR_INT);
    public static final DataComponentType<Integer> DAMAGE = wire("damage", Codec.INT, net.minecraft.network.codec.ByteBufCodecs.VAR_INT);
    public static final DataComponentType<Unit> UNBREAKABLE = wire("unbreakable", null, net.minecraft.util.Unit.STREAM_CODEC);
    public static final DataComponentType<UseEffects> USE_EFFECTS = type("use_effects");
    public static final DataComponentType<Component> CUSTOM_NAME = wire("custom_name", null, net.minecraft.network.chat.ComponentSerialization.STREAM_CODEC);
    public static final DataComponentType<Float> MINIMUM_ATTACK_CHARGE = wire("minimum_attack_charge", Codec.FLOAT, net.minecraft.network.codec.ByteBufCodecs.FLOAT);
    public static final DataComponentType<Holder<DamageType>> DAMAGE_TYPE = type("damage_type");
    public static final DataComponentType<Component> ITEM_NAME = wire("item_name", null, net.minecraft.network.chat.ComponentSerialization.STREAM_CODEC);
    public static final DataComponentType<Identifier> ITEM_MODEL = wire("item_model", null, Identifier.STREAM_CODEC);
    public static final DataComponentType<ItemLore> LORE = type("lore");
    public static final DataComponentType<Rarity> RARITY = wire("rarity", null, net.minecraft.network.codec.ByteBufCodecs.idMapper(id -> Rarity.values()[Math.floorMod(id, Rarity.values().length)], Rarity::ordinal));
    public static final DataComponentType<ItemEnchantments> ENCHANTMENTS = type("enchantments");
    public static final DataComponentType<AdventureModePredicate> CAN_PLACE_ON = type("can_place_on");
    public static final DataComponentType<AdventureModePredicate> CAN_BREAK = type("can_break");
    public static final DataComponentType<ItemAttributeModifiers> ATTRIBUTE_MODIFIERS = type("attribute_modifiers");
    public static final DataComponentType<CustomModelData> CUSTOM_MODEL_DATA = type("custom_model_data");
    public static final DataComponentType<TooltipDisplay> TOOLTIP_DISPLAY = type("tooltip_display");
    public static final DataComponentType<Integer> REPAIR_COST = wire("repair_cost", Codec.INT, net.minecraft.network.codec.ByteBufCodecs.VAR_INT);
    public static final DataComponentType<Unit> CREATIVE_SLOT_LOCK = wire("creative_slot_lock", null, net.minecraft.util.Unit.STREAM_CODEC);
    public static final DataComponentType<Boolean> ENCHANTMENT_GLINT_OVERRIDE = wire("enchantment_glint_override", Codec.BOOL, net.minecraft.network.codec.ByteBufCodecs.BOOL);
    public static final DataComponentType<Unit> INTANGIBLE_PROJECTILE = wire("intangible_projectile", null, net.minecraft.util.Unit.STREAM_CODEC);
    public static final DataComponentType<FoodProperties> FOOD = type("food");
    public static final DataComponentType<Consumable> CONSUMABLE = type("consumable");
    public static final DataComponentType<UseRemainder> USE_REMAINDER = type("use_remainder");
    public static final DataComponentType<UseCooldown> USE_COOLDOWN = type("use_cooldown");
    public static final DataComponentType<DamageResistant> DAMAGE_RESISTANT = type("damage_resistant");
    public static final DataComponentType<Tool> TOOL = type("tool");
    public static final DataComponentType<Weapon> WEAPON = type("weapon");
    public static final DataComponentType<AttackRange> ATTACK_RANGE = type("attack_range");
    public static final DataComponentType<Enchantable> ENCHANTABLE = type("enchantable");
    public static final DataComponentType<Equippable> EQUIPPABLE = type("equippable");
    public static final DataComponentType<Repairable> REPAIRABLE = type("repairable");
    public static final DataComponentType<Unit> GLIDER = type("glider");
    public static final DataComponentType<Identifier> TOOLTIP_STYLE = type("tooltip_style");
    public static final DataComponentType<DeathProtection> DEATH_PROTECTION = type("death_protection");
    public static final DataComponentType<BlocksAttacks> BLOCKS_ATTACKS = type("blocks_attacks");
    public static final DataComponentType<PiercingWeapon> PIERCING_WEAPON = type("piercing_weapon");
    public static final DataComponentType<KineticWeapon> KINETIC_WEAPON = type("kinetic_weapon");
    public static final DataComponentType<SwingAnimation> ATTACK_ANIMATION = type("attack_animation");
    public static final DataComponentType<SwingAnimation> INTERACT_ANIMATION = type("interact_animation");
    public static final DataComponentType<Integer> ADDITIONAL_TRADE_COST = type("additional_trade_cost", Codec.INT);
    public static final DataComponentType<Holder<BlockTransformer>> BLOCK_TRANSFORMER = type("block_transformer");
    public static final DataComponentType<VillagerFood> VILLAGER_FOOD = type("villager_food");
    public static final DataComponentType<ItemEnchantments> STORED_ENCHANTMENTS = type("stored_enchantments");
    public static final DataComponentType<DyeColor> DYE = type("dye");
    public static final DataComponentType<DyedItemColor> DYED_COLOR = type("dyed_color");
    public static final DataComponentType<MapId> MAP_ID = type("map_id");
    public static final DataComponentType<MapDecorations> MAP_DECORATIONS = type("map_decorations");
    public static final DataComponentType<MapPostProcessing> MAP_POST_PROCESSING = type("map_post_processing");
    public static final DataComponentType<ChargedProjectiles> CHARGED_PROJECTILES = type("charged_projectiles");
    public static final DataComponentType<BundleContents> BUNDLE_CONTENTS = type("bundle_contents");
    public static final DataComponentType<PotionContents> POTION_CONTENTS = type("potion_contents");
    public static final DataComponentType<Float> POTION_DURATION_SCALE = type("potion_duration_scale", Codec.FLOAT);
    public static final DataComponentType<SuspiciousStewEffects> SUSPICIOUS_STEW_EFFECTS = type("suspicious_stew_effects");
    public static final DataComponentType<WritableBookContent> WRITABLE_BOOK_CONTENT = type("writable_book_content");
    public static final DataComponentType<WrittenBookContent> WRITTEN_BOOK_CONTENT = type("written_book_content");
    public static final DataComponentType<ArmorTrim> TRIM = type("trim");
    public static final DataComponentType<DebugStickState> DEBUG_STICK_STATE = type("debug_stick_state");
    public static final DataComponentType<TypedEntityData<EntityType<?>>> ENTITY_DATA = wire("entity_data", null, net.minecraft.world.item.component.TypedEntityData.streamCodec(net.minecraft.world.entity.EntityType.STREAM_CODEC));
    public static final DataComponentType<CustomData> BUCKET_ENTITY_DATA = type("bucket_entity_data");
    public static final DataComponentType<TypedEntityData<BlockEntityType<?>>> BLOCK_ENTITY_DATA = wire("block_entity_data", null, net.minecraft.world.item.component.TypedEntityData.streamCodec(net.minecraft.network.codec.ByteBufCodecs.registry(net.minecraft.core.registries.Registries.BLOCK_ENTITY_TYPE)));
    public static final DataComponentType<InstrumentComponent> INSTRUMENT = type("instrument");
    public static final DataComponentType<Holder<TrimMaterial>> PROVIDES_TRIM_MATERIAL = type("provides_trim_material");
    public static final DataComponentType<OminousBottleAmplifier> OMINOUS_BOTTLE_AMPLIFIER = type("ominous_bottle_amplifier");
    public static final DataComponentType<JukeboxPlayable> JUKEBOX_PLAYABLE = type("jukebox_playable");
    public static final DataComponentType<HolderSet<BannerPattern>> PROVIDES_BANNER_PATTERNS = type("provides_banner_patterns");
    public static final DataComponentType<List<ResourceKey<Recipe<?>>>> RECIPES = type("recipes");
    public static final DataComponentType<LodestoneTracker> LODESTONE_TRACKER = type("lodestone_tracker");
    public static final DataComponentType<FireworkExplosion> FIREWORK_EXPLOSION = type("firework_explosion");
    public static final DataComponentType<Fireworks> FIREWORKS = type("fireworks");
    public static final DataComponentType<ResolvableProfile> PROFILE = type("profile");
    public static final DataComponentType<Identifier> NOTE_BLOCK_SOUND = type("note_block_sound");
    public static final DataComponentType<BannerPatternLayers> BANNER_PATTERNS = type("banner_patterns");
    public static final DataComponentType<DyeColor> BASE_COLOR = type("base_color");
    public static final DataComponentType<PotDecorations> POT_DECORATIONS = type("pot_decorations");
    public static final DataComponentType<ItemContainerContents> CONTAINER = type("container");
    public static final DataComponentType<BlockItemStateProperties> BLOCK_STATE = type("block_state");
    public static final DataComponentType<Bees> BEES = type("bees");
    public static final DataComponentType<SulfurCubeContent> SULFUR_CUBE_CONTENT = type("sulfur_cube_content");
    public static final DataComponentType<LockCode> LOCK = type("lock");
    public static final DataComponentType<SeededContainerLoot> CONTAINER_LOOT = type("container_loot");
    public static final DataComponentType<Holder<SoundEvent>> BREAK_SOUND = type("break_sound");
    public static final DataComponentType<Compostable> COMPOSTABLE = type("compostable");
    public static final DataComponentType<CookingFuel> COOKING_FUEL = type("cooking_fuel");
    public static final DataComponentType<BrewingFuel> BREWING_FUEL = type("brewing_fuel");
    public static final DataComponentType<MobVisibility> MOB_VISIBILITY = type("mob_visibility");
    public static final DataComponentType<Holder<VillagerType>> VILLAGER_VARIANT = type("villager_variant");
    public static final DataComponentType<Holder<WolfVariant>> WOLF_VARIANT = type("wolf_variant");
    public static final DataComponentType<Holder<WolfSoundVariant>> WOLF_SOUND_VARIANT = type("wolf_sound_variant");
    public static final DataComponentType<DyeColor> WOLF_COLLAR = type("wolf_collar");
    public static final DataComponentType<Fox.Variant> FOX_VARIANT = type("fox_variant");
    public static final DataComponentType<Salmon.Variant> SALMON_SIZE = type("salmon_size");
    public static final DataComponentType<Parrot.Variant> PARROT_VARIANT = type("parrot_variant");
    public static final DataComponentType<TropicalFish.Pattern> TROPICAL_FISH_PATTERN = type("tropical_fish_pattern");
    public static final DataComponentType<DyeColor> TROPICAL_FISH_BASE_COLOR = type("tropical_fish_base_color");
    public static final DataComponentType<DyeColor> TROPICAL_FISH_PATTERN_COLOR = type("tropical_fish_pattern_color");
    public static final DataComponentType<MushroomCow.Variant> MOOSHROOM_VARIANT = type("mooshroom_variant");
    public static final DataComponentType<Rabbit.Variant> RABBIT_VARIANT = type("rabbit_variant");
    public static final DataComponentType<Holder<PigVariant>> PIG_VARIANT = type("pig_variant");
    public static final DataComponentType<Holder<PigSoundVariant>> PIG_SOUND_VARIANT = type("pig_sound_variant");
    public static final DataComponentType<Holder<CowVariant>> COW_VARIANT = type("cow_variant");
    public static final DataComponentType<Holder<CowSoundVariant>> COW_SOUND_VARIANT = type("cow_sound_variant");
    public static final DataComponentType<Holder<ChickenVariant>> CHICKEN_VARIANT = type("chicken_variant");
    public static final DataComponentType<Holder<ChickenSoundVariant>> CHICKEN_SOUND_VARIANT = type("chicken_sound_variant");
    public static final DataComponentType<Holder<ZombieNautilusVariant>> ZOMBIE_NAUTILUS_VARIANT = type("zombie_nautilus_variant");
    public static final DataComponentType<Holder<FrogVariant>> FROG_VARIANT = type("frog_variant");
    public static final DataComponentType<Variant> HORSE_VARIANT = type("horse_variant");
    public static final DataComponentType<Holder<PaintingVariant>> PAINTING_VARIANT = type("painting_variant");
    public static final DataComponentType<Llama.Variant> LLAMA_VARIANT = type("llama_variant");
    public static final DataComponentType<Axolotl.Variant> AXOLOTL_VARIANT = type("axolotl_variant");
    public static final DataComponentType<Holder<CatVariant>> CAT_VARIANT = type("cat_variant");
    public static final DataComponentType<Holder<CatSoundVariant>> CAT_SOUND_VARIANT = type("cat_sound_variant");
    public static final DataComponentType<DyeColor> CAT_COLLAR = type("cat_collar");
    public static final DataComponentType<DyeColor> SHEEP_COLOR = type("sheep_color");
    public static final DataComponentType<DyeColor> SHULKER_COLOR = type("shulker_color");
    public static final DataComponentType<Holder<DecoratedPotPattern>> PROVIDES_POTTERY_PATTERN = type("provides_pottery_pattern");
    public static final DataComponentType<SignText> SIGN_TEXT_FRONT = type("sign_text_front");
    public static final DataComponentType<SignText> SIGN_TEXT_BACK = type("sign_text_back");
    public static final DataComponentType<Unit> WAXED = type("waxed");
    public static final DataComponentType<DyeColor> CUSHION_COLOR = type("cushion_color");
    public static final DataComponentMap COMMON_ITEM_COMPONENTS = DataComponentMap.EMPTY;
    public static DataComponentType<?> bootstrap(Registry<DataComponentType<?>> registry) {
        return CUSTOM_DATA;
    }
    private static final boolean kAttached = org.basaltmc.bukkit.facade.ReflectiveCodecs.attachAll(DataComponents.class);
}

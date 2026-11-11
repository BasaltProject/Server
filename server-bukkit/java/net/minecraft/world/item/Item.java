// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.resources.Identifier;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.Level;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.serialization.Codec;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.registries.BuiltInRegistries;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.component.DataComponentInitializers;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.TagKey;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.flag.FeatureFlag;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.food.FoodProperties;
import net.minecraft.world.inventory.ClickAction;
import net.minecraft.world.inventory.Slot;
import net.minecraft.world.inventory.tooltip.TooltipComponent;
import net.minecraft.world.item.component.Consumable;
import net.minecraft.world.item.component.ItemAttributeModifiers;
import net.minecraft.world.item.component.TooltipDisplay;
import net.minecraft.world.item.context.UseOnContext;
import net.minecraft.world.item.equipment.ArmorMaterial;
import net.minecraft.world.item.equipment.ArmorType;
import net.minecraft.world.item.equipment.trim.TrimMaterial;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.DecoratedPotPattern;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.saveddata.maps.MapId;
import net.minecraft.world.level.saveddata.maps.MapItemSavedData;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
import net.minecraft.world.phys.BlockHitResult;

public class Item implements ItemLike, FeatureElement {
    private final Identifier key;
    private int customMaxStack = -1;

    public Item(Identifier key) {
        this.key = key;
        this.descriptionId = "item." + key.getNamespace() + "." + key.getPath().replace('/', '.');
    }

    public Identifier itemKey() {
        return key;
    }

    public String getDescriptionId() {
        return descriptionId;
    }

    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        return InteractionResult.PASS;
    }

    @Override
    public String toString() {
        return key.toString();
    }
    public static final Codec<Holder<Item>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.Item.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<Item>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.Item.class, "STREAM_CODEC");
    public static final Codec<Holder<Item>> CODEC_WITH_BOUND_COMPONENTS = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.Item.class, "CODEC_WITH_BOUND_COMPONENTS");
    public static final Map<Block, Item> BY_BLOCK = new java.util.HashMap<>();
    public static final Identifier BASE_ATTACK_DAMAGE_ID = Identifier.withDefaultNamespace("base_attack_damage");
    public static final Identifier BASE_ATTACK_SPEED_ID = Identifier.withDefaultNamespace("base_attack_speed");
    public static final int DEFAULT_MAX_STACK_SIZE = 64;
    public static final int ABSOLUTE_MAX_STACK_SIZE = 99;
    public static final int MAX_BAR_WIDTH = 13;
    protected static final int APPROXIMATELY_INFINITE_USE_DURATION = 72000;
    protected final String descriptionId;
    public static int getId(Item item) {
        return BuiltInRegistries.ITEM.getId(item);
    }
    public static Item byId(int id) {
        return BuiltInRegistries.ITEM.byId(id);
    }
    public static Item byBlock(Block block) {
        return BuiltInRegistries.ITEM.getOptional(block.blockKey()).orElse(Items.AIR);
    }
    public Item(Properties properties) {
        this(properties.id != null ? properties.id : Identifier.withDefaultNamespace("air"));
        customMaxStack = properties.maxStackSize;
    }
    public Holder.Reference<Item> builtInRegistryHolder() {
        return net.minecraft.core.registries.BuiltInRegistries.ITEM.wrapAsHolder(this) instanceof Holder.Reference<Item> reference ? reference : null;
    }
    public DataComponentMap components() {
        return new ItemStack(this).getComponents();
    }
    public int getDefaultMaxStackSize() {
        return customMaxStack > 0 ? customMaxStack : org.basaltmc.bukkit.item.ItemData.of(key.toString()).maxStackSize();
    }
    public void onUseTick(Level level, LivingEntity livingEntity, ItemStack itemStack, int ticksRemaining) {

    }
    public void onDestroyed(ItemEntity itemEntity) {

    }
    public boolean canDestroyBlock(ItemStack itemStack, BlockState state, Level level, BlockPos pos, LivingEntity user) {
        return true;
    }
    public Item asItem() {
        return this;
    }
    public InteractionResult useOn(UseOnContext context) {
        return context.getItemInHand().useOn(context);
    }
    public float getDestroySpeed(ItemStack itemStack, BlockState state) {
        return 1.0F;
    }
    public ItemStack finishUsingItem(ItemStack itemStack, Level level, LivingEntity entity) {
        return itemStack;
    }
    public boolean isBarVisible(ItemStack stack) {
        return stack.isBarVisible();
    }
    public int getBarWidth(ItemStack stack) {
        return stack.getBarWidth();
    }
    public int getBarColor(ItemStack stack) {
        return stack.getBarColor();
    }
    public boolean overrideStackedOnOther(ItemStack self, Slot slot, ClickAction clickAction, Player player) {
        return false;
    }
    public boolean overrideOtherStackedOnMe(ItemStack self, ItemStack other, Slot slot, ClickAction clickAction, Player player, SlotAccess carriedItem) {
        return false;
    }
    public float getAttackDamageBonus(Entity victim, float damage, DamageSource damageSource) {
        return 0.0F;
    }
    public DamageSource getItemDamageSource(LivingEntity attacker) {
        return attacker.damageSources().generic();
    }
    public void hurtEnemy(ItemStack itemStack, LivingEntity mob, LivingEntity attacker) {

    }
    public void postHurtEnemy(ItemStack itemStack, LivingEntity mob, LivingEntity attacker) {

    }
    public boolean mineBlock(ItemStack itemStack, Level level, BlockState state, BlockPos pos, LivingEntity owner) {
        return false;
    }
    public boolean isCorrectToolForDrops(ItemStack itemStack, BlockState state) {
        return false;
    }
    public InteractionResult interactLivingEntity(ItemStack itemStack, Player player, LivingEntity target, InteractionHand type) {
        return InteractionResult.PASS;
    }
    public ItemStackTemplate getCraftingRemainder() {
        String remainder = org.basaltmc.bukkit.item.ItemData.of(key.toString()).craftingRemainder();
        if (remainder == null) {
            return null;
        }
        return net.minecraft.core.registries.BuiltInRegistries.ITEM.getOptional(Identifier.parse(remainder)).map(ItemStackTemplate::new).orElse(null);
    }
    public void inventoryTick(ItemStack itemStack, ServerLevel level, Entity owner, EquipmentSlot slot) {

    }
    public void onCraftedBy(ItemStack itemStack, Player player) {

    }
    public void onCraftedPostProcess(ItemStack itemStack, Level level) {

    }
    public ItemUseAnimation getUseAnimation(ItemStack itemStack) {
        return itemStack.getUseAnimation();
    }
    public int getUseDuration(ItemStack itemStack, LivingEntity user) {
        Consumable consumable = itemStack.get(net.minecraft.core.component.DataComponents.CONSUMABLE);
        if (consumable != null) {
            return consumable.consumeTicks();
        }
        switch (key.getPath()) {
            case "bow", "trident", "shield", "spyglass":
                return APPROXIMATELY_INFINITE_USE_DURATION;
            case "crossbow":
                return 28;
            case "brush":
                return 200;
            default:
                break;
        }
        Map<String, Object> food = org.basaltmc.bukkit.item.ItemData.of(key.toString()).food();
        if (food != null && food.get("consume_seconds") instanceof Number seconds) {
            return (int) (seconds.floatValue() * 20.0F);
        }
        return 0;
    }
    public boolean releaseUsing(ItemStack itemStack, Level level, LivingEntity entity, int remainingTime) {
        return false;
    }
    public void appendHoverText(ItemStack itemStack, TooltipContext context, TooltipDisplay display, Consumer<Component> builder, TooltipFlag tooltipFlag) {

    }
    public Optional<TooltipComponent> getTooltipImage(ItemStack itemStack) {
        return Optional.empty();
    }
    public Component getName(ItemStack itemStack) {
        return Component.translatable(getDescriptionId());
    }
    public boolean isFoil(ItemStack itemStack) {
        return false;
    }
    protected static BlockHitResult getPlayerPOVHitResult(Level level, Player player, ClipContext.Fluid fluid) {
        net.minecraft.world.phys.Vec3 eye = player.getEyePosition();
        net.minecraft.world.phys.Vec3 look = player.getViewVector(1.0F);
        double reach = player.blockInteractionRange();
        net.minecraft.world.phys.Vec3 end = eye.add(look.x * reach, look.y * reach, look.z * reach);
        org.bukkit.FluidCollisionMode mode = switch (fluid) {
            case NONE -> org.bukkit.FluidCollisionMode.NEVER;
            case SOURCE_ONLY -> org.bukkit.FluidCollisionMode.SOURCE_ONLY;
            default -> org.bukkit.FluidCollisionMode.ALWAYS;
        };
        org.bukkit.util.RayTraceResult hit = org.basaltmc.bukkit.misc.RayTracing.blocks(((ServerLevel) level).getWorld(),
            new org.bukkit.util.Vector(eye.x, eye.y, eye.z), new org.bukkit.util.Vector(look.x, look.y, look.z), reach, mode, false, null);
        if (hit == null || hit.getHitBlock() == null) {
            return BlockHitResult.miss(end, net.minecraft.core.Direction.getApproximateNearest(look), BlockPos.containing(end));
        }
        org.bukkit.block.Block block = hit.getHitBlock();
        org.bukkit.util.Vector at = hit.getHitPosition();
        net.minecraft.core.Direction face = hit.getHitBlockFace() == null ? net.minecraft.core.Direction.UP
            : net.minecraft.core.Direction.valueOf(hit.getHitBlockFace().name());
        return new BlockHitResult(new net.minecraft.world.phys.Vec3(at.getX(), at.getY(), at.getZ()), face,
            new BlockPos(block.getX(), block.getY(), block.getZ()), false);
    }
    public boolean useOnRelease(ItemStack itemStack) {
        return false;
    }
    public ItemStack getDefaultInstance() {
        return new ItemStack(this);
    }
    public boolean canFitInsideContainerItems() {
        return true;
    }
    public FeatureFlagSet requiredFeatures() {
        return FeatureFlagSet.of();
    }
    public boolean shouldPrintOpWarning(ItemStack stack, Player player) {
        return false;
    }
    public static class Properties {
        private int maxStackSize = -1;
        private int maxDamage;
        private Identifier id;
        private String descriptionOverride;
        private String descriptionPrefix = "item";
        public Properties() {
        }
        public Properties food(FoodProperties foodProperties) {
            return this;
        }
        public Properties food(FoodProperties foodProperties, Consumable consumable) {
            return this;
        }
        public Properties villagerFood(int nutrition) {
            return this;
        }
        public Properties usingConvertsTo(Item item) {
            return this;
        }
        public Properties useCooldown(float seconds) {
            return this;
        }
        public Properties stacksTo(int max) {
            this.maxStackSize = max;
            return this;
        }
        public Properties durability(int maxDamage) {
            this.maxDamage = maxDamage;
            this.maxStackSize = 1;
            return this;
        }
        public Properties craftRemainder(Item craftingRemainingItem) {
            return this;
        }
        public Properties craftRemainder(ItemStackTemplate craftingRemainingItem) {
            return this;
        }
        public Properties rarity(Rarity rarity) {
            return this;
        }
        public Properties fireResistant() {
            return this;
        }
        public Properties jukeboxPlayable(ResourceKey<JukeboxSong> song) {
            return this;
        }
        public Properties enchantable(int value) {
            return this;
        }
        public Properties potPattern(ResourceKey<DecoratedPotPattern> pattern) {
            return this;
        }
        public Properties repairable(Item repairItem) {
            return this;
        }
        public Properties repairable(TagKey<Item> repairItems) {
            return this;
        }
        public Properties equippable(EquipmentSlot slot) {
            return this;
        }
        public Properties equippableUnswappable(EquipmentSlot slot) {
            return this;
        }
        public Properties tool(ToolMaterial material, TagKey<Block> minesEfficiently, float attackDamageBaseline, float attackSpeedBaseline, float disableBlockingSeconds) {
            return this;
        }
        public Properties pickaxe(ToolMaterial material, float attackDamageBaseline, float attackSpeedBaseline) {
            return this;
        }
        public Properties axe(ToolMaterial material, float attackDamageBaseline, float attackSpeedBaseline) {
            return this;
        }
        public Properties hoe(ToolMaterial material, float attackDamageBaseline, float attackSpeedBaseline) {
            return this;
        }
        public Properties shovel(ToolMaterial material, float attackDamageBaseline, float attackSpeedBaseline) {
            return this;
        }
        public Properties sword(ToolMaterial material, float attackDamageBaseline, float attackSpeedBaseline) {
            return this;
        }
        public Properties spear(ToolMaterial material, float attackDuration, float damageMultiplier, float delay, float dismountTime, float dismountThreshold, float knockbackTime, float knockbackThreshold, float damageTime, float damageThreshold) {
            return this;
        }
        public Properties spawnEgg(EntityType<?> type) {
            return this;
        }
        public Properties humanoidArmor(ArmorMaterial material, ArmorType type) {
            return this;
        }
        public Properties wolfArmor(ArmorMaterial material) {
            return this;
        }
        public Properties horseArmor(ArmorMaterial material) {
            return this;
        }
        public Properties nautilusArmor(ArmorMaterial material) {
            return this;
        }
        public Properties trimMaterial(ResourceKey<TrimMaterial> material) {
            return this;
        }
        public Properties signText() {
            return this;
        }
        public Properties cookingFuel(ResourceKey<ContextIntProvider> burnTime) {
            return this;
        }
        public Properties brewingFuel(ResourceKey<ContextIntProvider> uses) {
            return this;
        }
        public Properties requiredFeatures(FeatureFlag... flags) {
            return this;
        }
        public Properties requiredFeatures(FeatureFlagSet flags) {
            return this;
        }
        public Properties setId(ResourceKey<Item> id) {
            this.id = id.identifier();
            return this;
        }
        public Properties overrideDescription(String descriptionId) {
            this.descriptionOverride = descriptionId;
            return this;
        }
        public Properties useBlockDescriptionPrefix() {
            this.descriptionPrefix = "block";
            return this;
        }
        public Properties useItemDescriptionPrefix() {
            this.descriptionPrefix = "item";
            return this;
        }
        protected String effectiveDescriptionId() {
            if (descriptionOverride != null) {
                return descriptionOverride;
            }
            return id == null ? "" : descriptionPrefix + "." + id.getNamespace() + "." + id.getPath().replace('/', '.');
        }
        public Identifier effectiveModel() {
            return id;
        }
        public <T> Properties component(DataComponentType<T> type, T value) {
            return this;
        }
        public <T> Properties delayedComponent(DataComponentType<T> type, DataComponentInitializers.SingleComponentInitializer<T> initializer) {
            return this;
        }
        public <T> Properties delayedHolderComponent(DataComponentType<Holder<T>> type, ResourceKey<T> valueKey) {
            return this;
        }
        public Properties attributes(ItemAttributeModifiers attributes) {
            return this;
        }
        public Properties compostable(ResourceKey<ContextIntProvider> layers) {
            return this;
        }
        public final Properties loweredMobVisibility(ResourceKey<EntityType<?>>... entityTypeIds) {
            return this;
        }
    }
    public interface TooltipContext {
        TooltipContext EMPTY = new TooltipContext() {
            public HolderLookup.Provider registries() {
                return null;
            }
            public float tickRate() {
                return 20.0F;
            }
            public MapItemSavedData mapData(MapId id) {
                return null;
            }
            public boolean isPeaceful() {
                return false;
            }
        };
        HolderLookup. Provider registries();
        float tickRate();
        MapItemSavedData mapData(MapId id);
        boolean isPeaceful();
        static TooltipContext of(Level level) {
            return level == null ? EMPTY : new TooltipContext() {
            public HolderLookup.Provider registries() {
                return level.registryAccess();
            }
            public float tickRate() {
                return 20.0F;
            }
            public MapItemSavedData mapData(MapId id) {
                return level.getMapData(id);
            }
            public boolean isPeaceful() {
                return level.getDifficulty() == net.minecraft.world.Difficulty.PEACEFUL;
            }
        };
        }
        static TooltipContext of(HolderLookup.Provider registries) {
            return new TooltipContext() {
            public HolderLookup.Provider registries() {
                return registries;
            }
            public float tickRate() {
                return 20.0F;
            }
            public MapItemSavedData mapData(MapId id) {
                return null;
            }
            public boolean isPeaceful() {
                return false;
            }
        };
        }
    }
}

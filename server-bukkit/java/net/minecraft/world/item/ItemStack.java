// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;
import net.minecraft.world.item.component.Consumable;

import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.item.context.UseOnContext;
import net.minecraft.world.level.ItemLike;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.function.BiConsumer;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.function.UnaryOperator;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentHolder;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.core.component.PatchedDataComponentMap;
import net.minecraft.core.component.TypedDataComponent;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.inventory.ClickAction;
import net.minecraft.world.inventory.Slot;
import net.minecraft.world.inventory.tooltip.TooltipComponent;
import net.minecraft.world.item.component.ItemAttributeModifiers;
import net.minecraft.world.item.component.SwingAnimation;
import net.minecraft.world.item.component.TooltipDisplay;
import net.minecraft.world.item.component.TooltipProvider;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.ItemEnchantments;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;
import net.minecraft.world.level.gameevent.GameEvent;
import org.apache.commons.lang3.function.TriConsumer;

public class ItemStack implements DataComponentHolder, ItemInstance {
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemStack> OPTIONAL_STREAM_CODEC = StreamCodec.of(
        (buf, stack) -> {
            if (stack.isEmpty()) {
                buf.writeVarInt(0);
                return;
            }
            writeStack(buf, stack);
        },
        buf -> {
            int count = buf.readVarInt();
            return count <= 0 ? ItemStack.EMPTY : readStack(buf, count);
        });
    public static final ItemStack EMPTY = new ItemStack((Item) null, 0);

    private int popTime;
    private Item item;
    private int count;
    private DataComponentPatch patch = DataComponentPatch.EMPTY;

    public ItemStack(ItemLike item, int count) {
        this.item = item instanceof Item found ? found : null;
        this.count = count;
    }

    public ItemStack(Holder<Item> item, int count) {
        this.item = item == null ? null : item.value();
        this.count = count;
    }

    @Override
    public <T> T get(DataComponentType<? extends T> type) {
        return patch.get(DataComponentMap.EMPTY, type);
    }

    public Item getItem() {
        return item == null ? BuiltInRegistries.ITEM.getValue(BuiltInRegistries.ITEM.getDefaultKey()) : item;
    }

    public int getCount() {
        return count;
    }

    public void setCount(int count) {
        this.count = count;
    }

    public boolean isEmpty() {
        return item == null || count <= 0;
    }

    public DataComponentPatch getComponentsPatch() {
        return patch;
    }

    public void applyComponents(DataComponentPatch changes) {
        patch = changes == null ? DataComponentPatch.EMPTY : changes;
    }

    public Component getItemName() {
        return Component.translatable(getItem().getDescriptionId());
    }

    public InteractionResult useOn(UseOnContext context) {
        if (isEmpty()) {
            return InteractionResult.PASS;
        }
        return org.basaltmc.bukkit.facade.Effects.useItem(this, (net.minecraft.server.level.ServerLevel) context.getLevel(), context.getHitResult())
            ? InteractionResult.SUCCESS : InteractionResult.PASS;
    }
    public static final MapCodec<ItemStack> MAP_CODEC = ItemStackTemplate.MAP_CODEC.xmap(ItemStackTemplate::create, ItemStackTemplate::fromNonEmptyStack);
    public static final Codec<ItemStack> CODEC = MAP_CODEC.codec();
    public static final Codec<ItemStack> OPTIONAL_CODEC = new Codec<>() {
        @Override
        public <T> DataResult<com.mojang.datafixers.util.Pair<ItemStack, T>> decode(com.mojang.serialization.DynamicOps<T> ops, T input) {
            DataResult<com.mojang.datafixers.util.Pair<ItemStack, T>> result = CODEC.decode(ops, input);
            return result.error().isPresent() && ops.getMap(input).result().map(map -> map.entries().findAny().isEmpty()).orElse(false)
                ? DataResult.success(com.mojang.datafixers.util.Pair.of(ItemStack.EMPTY, input)) : result;
        }

        @Override
        public <T> DataResult<T> encode(ItemStack input, com.mojang.serialization.DynamicOps<T> ops, T prefix) {
            return input.isEmpty() ? DataResult.success(ops.emptyMap()) : CODEC.encode(input, ops, prefix);
        }
    };
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemStack> OPTIONAL_UNTRUSTED_STREAM_CODEC = OPTIONAL_STREAM_CODEC;
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemStack> STREAM_CODEC = StreamCodec.of(
        (buf, stack) -> {
            if (stack.isEmpty()) {
                throw new io.netty.handler.codec.EncoderException("Empty ItemStack not allowed");
            }
            writeStack(buf, stack);
        },
        buf -> {
            int count = buf.readVarInt();
            if (count <= 0) {
                throw new io.netty.handler.codec.DecoderException("Empty ItemStack not allowed");
            }
            return readStack(buf, count);
        });
    public static final StreamCodec<RegistryFriendlyByteBuf, List<ItemStack>> OPTIONAL_LIST_STREAM_CODEC = OPTIONAL_STREAM_CODEC.apply(net.minecraft.network.codec.ByteBufCodecs.collection(java.util.ArrayList::new));
    private static void writeStack(RegistryFriendlyByteBuf buf, ItemStack stack) {
        buf.writeVarInt(stack.getCount());
        buf.writeVarInt(BuiltInRegistries.ITEM.getId(stack.getItem()));
        DataComponentPatch.write(buf, stack.getComponentsPatch());
    }

    private static ItemStack readStack(RegistryFriendlyByteBuf buf, int count) {
        Item item = BuiltInRegistries.ITEM.byId(buf.readVarInt());
        ItemStack stack = new ItemStack(item, count);
        stack.applyComponents(DataComponentPatch.read(buf));
        return stack;
    }

    public static DataResult<ItemStack> validateStrict(ItemStack itemStack) {
        return DataResult.success(itemStack);
    }
    public static StreamCodec<RegistryFriendlyByteBuf, ItemStack> validatedStreamCodec(StreamCodec<RegistryFriendlyByteBuf, ItemStack> codec) {
        return codec;
    }
    public Optional<TooltipComponent> getTooltipImage() {
        return Optional.empty();
    }
    public DataComponentMap getComponents() {
        return PatchedDataComponentMap.fromPatch(DataComponentMap.EMPTY, patch);
    }
    public DataComponentMap getPrototype() {
        return DataComponentMap.EMPTY;
    }
    public DataComponentMap immutableComponents() {
        return DataComponentMap.EMPTY;
    }
    public boolean hasNonDefault(DataComponentType<?> type) {
        return patch.split().added().has(type);
    }
    public ItemStack(ItemLike item) {
        this(item, 1);
    }
    public ItemStack(Holder<Item> item) {
        this(item, 1);
    }
    public ItemStack(Holder<Item> item, int count, DataComponentPatch components) {
        this(item, count);
        this.patch = components == null ? DataComponentPatch.EMPTY : components;
    }
    private ItemStack(Holder<Item> item, int count, PatchedDataComponentMap components) {
        this(item, count);
        this.patch = components.asPatch();
    }
    private ItemStack(Void nullMarker) {
        this((Item) null, 0);
    }
    public boolean isItemEnabled(FeatureFlagSet enabledFeatures) {
        return true;
    }
    public ItemStack split(int amount) {
        int taken = Math.min(amount, getCount());
        ItemStack result = copyWithCount(taken);
        shrink(taken);
        return result;
    }
    public ItemStack copyAndClear() {
        if (isEmpty()) {
            return EMPTY;
        }
        ItemStack result = copy();
        setCount(0);
        return result;
    }
    public Holder<Item> typeHolder() {
        return BuiltInRegistries.ITEM.wrapAsHolder(getItem());
    }
    public boolean is(Predicate<Holder<Item>> item) {
        return item.test(typeHolder());
    }
    public float getDestroySpeed(BlockState state) {
        double[] facts = org.basaltmc.bukkit.WorldEnvNatives.toolFacts(isEmpty() ? "" : getItem().itemKey().toString(), state.stateId());
        return facts == null || facts.length < 2 ? 1.0F : (float) facts[1];
    }
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        return InteractionResult.PASS;
    }
    public ItemStack finishUsingItem(Level level, LivingEntity livingEntity) {
        return this;
    }
    public boolean isStackable() {
        return getMaxStackSize() > 1;
    }
    public boolean isDamageableItem() {
        return !isEmpty() && getMaxDamage() > 0;
    }
    public boolean isDamaged() {
        return isDamageableItem() && getDamageValue() > 0;
    }
    public int getDamageValue() {
        Integer damage = get(DataComponents.DAMAGE);
        return damage == null ? 0 : Math.max(0, Math.min(damage, getMaxDamage()));
    }
    public void setDamageValue(int value) {
        set(DataComponents.DAMAGE, Math.max(0, value));
    }
    public int getMaxDamage() {
        Integer max = get(DataComponents.MAX_DAMAGE);
        return max != null ? max : org.basaltmc.bukkit.item.ItemData.of(getItem().itemKey().toString()).maxDamage();
    }
    public boolean isBroken() {
        return isDamageableItem() && getDamageValue() >= getMaxDamage();
    }
    public boolean nextDamageWillBreak() {
        return isDamageableItem() && getDamageValue() >= getMaxDamage() - 1;
    }
    public void hurtAndBreak(int amount, ServerLevel level, ServerPlayer player, Consumer<ItemStack> onBreak) {
        if (processDamage(level, amount)) {
            ItemStack broken = copy();
            shrink(1);
            onBreak.accept(broken);
        }
    }
    public void hurtWithoutBreaking(int amount, Player player) {
        if (isDamageableItem()) {
            setDamageValue(Math.min(getDamageValue() + amount, getMaxDamage() - 1));
        }
    }
    public void hurtAndBreak(int amount, LivingEntity owner, InteractionHand hand) {
        if (processDamage(owner.level() instanceof ServerLevel server ? server : null, amount)) {
            shrink(1);
        }
    }
    public void hurtAndBreak(int amount, LivingEntity owner, EquipmentSlot slot) {
        if (processDamage(owner.level() instanceof ServerLevel server ? server : null, amount)) {
            shrink(1);
        }
    }
    public ItemStack hurtAndConvertOnBreak(int amount, ItemLike newItem, LivingEntity owner, EquipmentSlot slot) {
        if (!processDamage(owner.level() instanceof ServerLevel server ? server : null, amount)) {
            return this;
        }
        ItemStack converted = transmuteCopy(newItem);
        shrink(1);
        return converted;
    }
    public boolean isBarVisible() {
        return isDamaged();
    }
    public int getBarWidth() {
        return net.minecraft.util.Mth.clamp(Math.round(13.0F - getDamageValue() * 13.0F / getMaxDamage()), 0, 13);
    }
    public int getBarColor() {
        float fraction = Math.max(0.0F, (getMaxDamage() - getDamageValue()) / (float) getMaxDamage());
        float hue = fraction / 3.0F;
        int red = (int) (255.0F * Math.clamp(Math.abs(hue * 6.0F - 3.0F) - 1.0F, 0.0F, 1.0F));
        int green = (int) (255.0F * Math.clamp(2.0F - Math.abs(hue * 6.0F - 2.0F), 0.0F, 1.0F));
        int blue = (int) (255.0F * Math.clamp(2.0F - Math.abs(hue * 6.0F - 4.0F), 0.0F, 1.0F));
        return 0xFF000000 | red << 16 | green << 8 | blue;
    }
    public boolean overrideStackedOnOther(Slot slot, ClickAction clickAction, Player player) {
        return false;
    }
    public boolean overrideOtherStackedOnMe(ItemStack other, Slot slot, ClickAction clickAction, Player player, SlotAccess carriedItem) {
        return false;
    }
    public boolean hurtEnemy(LivingEntity mob, LivingEntity attacker) {
        return false;
    }
    public void postHurtEnemy(LivingEntity mob, LivingEntity attacker) {
        return;
    }
    public void mineBlock(Level level, BlockState state, BlockPos pos, Player owner) {
        return;
    }
    public boolean isCorrectToolForDrops(BlockState state) {
        double[] facts = org.basaltmc.bukkit.WorldEnvNatives.toolFacts(isEmpty() ? "" : getItem().itemKey().toString(), state.stateId());
        return facts != null && facts.length > 0 && facts[0] != 0.0;
    }
    public InteractionResult interactLivingEntity(Player player, LivingEntity target, InteractionHand hand) {
        return InteractionResult.PASS;
    }
    public ItemStack copy() {
        if (isEmpty()) {
            return EMPTY;
        }
        ItemStack result = new ItemStack(this.item, count);
        result.patch = patch;
        return result;
    }
    public ItemStack copyWithCount(int count) {
        if (isEmpty()) {
            return EMPTY;
        }
        ItemStack result = copy();
        result.setCount(count);
        return result;
    }
    public ItemStack transmuteCopy(ItemLike newItem) {
        return transmuteCopy(newItem, getCount());
    }
    public ItemStack transmuteCopy(ItemLike newItem, int newCount) {
        if (isEmpty()) {
            return EMPTY;
        }
        ItemStack result = new ItemStack(newItem, newCount);
        result.patch = patch;
        return result;
    }
    public static boolean matches(ItemStack a, ItemStack b) {
        if (a == b) {
            return true;
        }
        return a.getCount() == b.getCount() && isSameItemSameComponents(a, b);
    }
    public static boolean listMatches(List<ItemStack> left, List<ItemStack> right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (int i = 0; i < left.size(); i++) {
            if (!matches(left.get(i), right.get(i))) {
                return false;
            }
        }
        return true;
    }
    public static boolean isSameItem(ItemStack a, ItemStack b) {
        return a.is(b.getItem());
    }
    public static boolean isSameItemSameComponents(ItemStack a, ItemStack b) {
        return a.is(b.getItem()) && (a.isEmpty() && b.isEmpty() || java.util.Objects.equals(a.patch, b.patch));
    }
    public static boolean matchesIgnoringComponents(ItemStack a, ItemStack b, Predicate<DataComponentType<?>> ignoredPredicate) {
        return a.getItem() == b.getItem() && a.getComponentsPatch().forget(ignoredPredicate).equals(b.getComponentsPatch().forget(ignoredPredicate));
    }
    public static MapCodec<ItemStack> lenientOptionalFieldOf(String name) {
        return CODEC.lenientOptionalFieldOf(name).xmap(stack -> stack.orElse(EMPTY),
            stack -> stack.isEmpty() ? java.util.Optional.empty() : java.util.Optional.of(stack));
    }
    public static int hashItemAndComponents(ItemStack item) {
        return item == null || item.isEmpty() ? 0 : 31 * item.getItem().hashCode() + item.patch.hashCode();
    }
    public static int hashStackList(List<ItemStack> items) {
        int hash = 0;
        for (ItemStack stack : items) {
            hash = hash * 31 + hashItemAndComponents(stack);
        }
        return hash;
    }
    public String toString() {
        return count + " " + getItem().itemKey();
    }
    public void inventoryTick(Level level, Entity owner, EquipmentSlot slot) {
        return;
    }
    public void onCraftedBy(Player player, int craftCount) {
        return;
    }
    public void onCraftedBySystem(Level level) {
        return;
    }
    public int getUseDuration(LivingEntity user) {
        return isEmpty() ? 0 : getItem().getUseDuration(this, user);
    }
    public ItemUseAnimation getUseAnimation() {
        Consumable consumable = get(net.minecraft.core.component.DataComponents.CONSUMABLE);
        if (consumable != null) {
            return consumable.animation();
        }
        String path = getItem().itemKey().getPath();
        return switch (path) {
            case "bow" -> ItemUseAnimation.BOW;
            case "crossbow" -> ItemUseAnimation.CROSSBOW;
            case "trident" -> ItemUseAnimation.TRIDENT;
            case "shield" -> ItemUseAnimation.BLOCK;
            case "spyglass" -> ItemUseAnimation.SPYGLASS;
            case "goat_horn" -> ItemUseAnimation.TOOT_HORN;
            case "brush" -> ItemUseAnimation.BRUSH;
            default -> org.basaltmc.bukkit.item.ItemData.of(getItem().itemKey().toString()).food() != null ? ItemUseAnimation.EAT : ItemUseAnimation.NONE;
        };
    }
    public void releaseUsing(Level level, LivingEntity entity, int remainingTime) {
        return;
    }
    public void causeUseVibration(Entity causer, Holder.Reference<GameEvent> event) {
        return;
    }
    public boolean useOnRelease() {
        return false;
    }
    public <T> T set(DataComponentType<T> type, T value) {
        PatchedDataComponentMap map = PatchedDataComponentMap.fromPatch(DataComponentMap.EMPTY, patch);
        T previous = map.set(type, value);
        patch = map.asPatch();
        return previous;
    }
    public <T> T set(TypedDataComponent<T> value) {
        return set(value.type(), value.value());
    }
    public <T> void copyFrom(DataComponentType<T> type, DataComponentGetter source) {
        T value = source.get(type);
        if (value != null) {
            set(type, value);
        }
    }
    public <T, U> T update(DataComponentType<T> type, T defaultValue, U value, BiFunction<T, U, T> combiner) {
        return update(type, defaultValue, current -> combiner.apply(current, value));
    }
    public <T> T update(DataComponentType<T> type, T defaultValue, UnaryOperator<T> function) {
        T current = getOrDefault(type, defaultValue);
        T updated = function.apply(current);
        set(type, updated);
        return updated;
    }
    public <T> T remove(DataComponentType<? extends T> type) {
        PatchedDataComponentMap map = PatchedDataComponentMap.fromPatch(DataComponentMap.EMPTY, patch);
        T previous = map.remove(type);
        patch = map.asPatch();
        return previous;
    }
    public void applyComponentsAndValidate(DataComponentPatch patch) {
        applyComponents(patch);
    }
    public void applyComponents(DataComponentMap components) {
        PatchedDataComponentMap map = PatchedDataComponentMap.fromPatch(DataComponentMap.EMPTY, patch);
        map.setAll(components);
        patch = map.asPatch();
    }
    public Component getHoverName() {
        return getCustomName() != null ? getCustomName() : getItemName();
    }
    public Component getCustomName() {
        return get(DataComponents.CUSTOM_NAME);
    }
    public Component getStyledHoverName() {
        return getHoverName();
    }
    public <T> void addToTooltip(DataComponentType<T> type, TooltipProvider.Getter<T> tooltipGetter, Item.TooltipContext context, TooltipDisplay display, Consumer<Component> consumer, TooltipFlag flag) {
        T component = get(type);
        if (component != null && display.shows(type)) {
            tooltipGetter.get(component).addToTooltip(context, consumer, flag, this);
        }
    }
    public <T extends TooltipProvider> void addToTooltip(DataComponentType<T> type, Item.TooltipContext context, TooltipDisplay display, Consumer<Component> consumer, TooltipFlag flag) {
        T component = get(type);
        if (component != null && display.shows(type)) {
            component.addToTooltip(context, consumer, flag, this);
        }
    }
    public List<Component> getTooltipLines(Item.TooltipContext context, Player player, TooltipFlag tooltipFlag) {
        return new ArrayList<>(List.of(getHoverName()));
    }
    public void addDetailsToTooltip(Item.TooltipContext context, TooltipDisplay display, Player player, TooltipFlag tooltipFlag, Consumer<Component> builder) {
        return;
    }
    public boolean hasFoil() {
        return false;
    }
    public Rarity getRarity() {
        return Rarity.COMMON;
    }
    public boolean isEnchantable() {
        return has(DataComponents.ENCHANTABLE) && !isEnchanted();
    }
    public void enchant(Holder<Enchantment> enchantment, int level) {
        ItemEnchantments.Mutable mutable = new ItemEnchantments.Mutable(getEnchantments());
        mutable.set(enchantment, level);
        set(DataComponents.ENCHANTMENTS, mutable.toImmutable());
    }
    public boolean isEnchanted() {
        return !getEnchantments().isEmpty();
    }
    public ItemEnchantments getEnchantments() {
        return getOrDefault(DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY);
    }
    public void forEachModifier(EquipmentSlotGroup slot, TriConsumer<Holder<Attribute>, AttributeModifier, ItemAttributeModifiers.Display> consumer) {
        getOrDefault(DataComponents.ATTRIBUTE_MODIFIERS, ItemAttributeModifiers.EMPTY).forEach(slot, consumer);
    }
    public void forEachModifier(EquipmentSlot slot, BiConsumer<Holder<Attribute>, AttributeModifier> consumer) {
        getOrDefault(DataComponents.ATTRIBUTE_MODIFIERS, ItemAttributeModifiers.EMPTY).forEach(slot, consumer);
    }
    public Component getDisplayName() {
        return getHoverName();
    }
    public SwingAnimation getAttackAnimation() {
        return getOrDefault(net.minecraft.core.component.DataComponents.ATTACK_ANIMATION, net.minecraft.world.item.component.SwingAnimation.DEFAULT);
    }
    public SwingAnimation getInteractAnimation() {
        return getOrDefault(net.minecraft.core.component.DataComponents.INTERACT_ANIMATION, net.minecraft.world.item.component.SwingAnimation.DEFAULT);
    }
    public boolean canPlaceOnBlockInAdventureMode(BlockInWorld blockInWorld) {
        return false;
    }
    public boolean canBreakBlockInAdventureMode(BlockInWorld blockInWorld) {
        return false;
    }
    public int getPopTime() {
        return popTime;
    }
    public void setPopTime(int popTime) {
        this.popTime = popTime;
    }
    public int count() {
        return count;
    }
    public void limitSize(int maxStackSize) {
        if (!isEmpty() && getCount() > maxStackSize) {
            setCount(maxStackSize);
        }
    }
    public void grow(int amount) {
        setCount(getCount() + amount);
    }
    public void shrink(int amount) {
        grow(-amount);
    }
    public void consume(int amount, LivingEntity owner) {
        shrink(amount);
    }
    public ItemStack consumeAndReturn(int amount, LivingEntity owner) {
        ItemStack taken = copyWithCount(amount);
        shrink(amount);
        return taken;
    }
    public void onUseTick(Level level, LivingEntity livingEntity, int ticksRemaining) {
        return;
    }
    public void onDestroyed(ItemEntity itemEntity) {
        return;
    }
    public boolean canBeHurtBy(DamageSource source) {
        return true;
    }
    public boolean isValidRepairItem(ItemStack repairItem) {
        return false;
    }
    public boolean canDestroyBlock(BlockState state, Level level, BlockPos pos, Player player) {
        return true;
    }
    public DamageSource getDamageSource(LivingEntity attacker) {
        return null;
    }
    private boolean processDamage(ServerLevel level, int amount) {
        if (!isDamageableItem() || amount <= 0) {
            return false;
        }
        int processed = amount;
        if (level != null) {
            org.apache.commons.lang3.mutable.MutableFloat change = new org.apache.commons.lang3.mutable.MutableFloat(amount);
            for (it.unimi.dsi.fastutil.objects.Object2IntMap.Entry<Holder<Enchantment>> entry : getEnchantments().entrySet()) {
                entry.getKey().value().modifyDurabilityChange(level, entry.getIntValue(), this, change);
            }
            processed = (int) change.floatValue();
        }
        if (processed <= 0) {
            return false;
        }
        setDamageValue(getDamageValue() + processed);
        return getDamageValue() >= getMaxDamage();
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.Unit;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.effects.EnchantmentAttributeEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentEntityEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentValueEffect;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.phys.Vec3;
import org.apache.commons.lang3.mutable.MutableFloat;
public record Enchantment(Component description, EnchantmentDefinition definition, HolderSet<Enchantment> exclusiveSet, DataComponentMap effects) {
    public static final int MAX_LEVEL = 255;
    public static final Codec<Enchantment> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.Enchantment.class, "DIRECT_CODEC");
    public static final Codec<Holder<Enchantment>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.Enchantment.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<Enchantment>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.enchantment.Enchantment.class, "STREAM_CODEC");
    public static Cost constantCost(int base) {
        return new Cost(base, 0);
    }
    public static Cost dynamicCost(int base, int perLevel) {
        return new Cost(base, perLevel);
    }
    public static EnchantmentDefinition definition(HolderSet<Item> supportedItems, HolderSet<Item> primaryItems, int weight, int maxLevel, Cost minCost, Cost maxCost, int anvilCost, EquipmentSlotGroup... slots) {
        return new EnchantmentDefinition(supportedItems, Optional.of(primaryItems), weight, maxLevel, minCost, maxCost, anvilCost,
            List.of(slots));
    }
    public static EnchantmentDefinition definition(HolderSet<Item> supportedItems, int weight, int maxLevel, Cost minCost, Cost maxCost, int anvilCost, EquipmentSlotGroup... slots) {
        return new EnchantmentDefinition(supportedItems, Optional.empty(), weight, maxLevel, minCost, maxCost, anvilCost,
            List.of(slots));
    }
    public Map<EquipmentSlot, ItemStack> getSlotItems(LivingEntity entity) {
        Map<EquipmentSlot, ItemStack> found = new java.util.EnumMap<>(EquipmentSlot.class);
        for (EquipmentSlot slot : EquipmentSlot.VALUES) {
            if (matchingSlot(slot)) {
                ItemStack stack = entity.getItemBySlot(slot);
                if (!stack.isEmpty()) {
                    found.put(slot, stack);
                }
            }
        }
        return found;
    }
    public HolderSet<Item> getSupportedItems() {
        return definition.supportedItems();
    }
    public boolean matchingSlot(EquipmentSlot slot) {
        return definition.slots().stream().anyMatch(group -> group.test(slot));
    }
    public boolean isPrimaryItem(ItemStack item) {
        return isSupportedItem(item) && definition.primaryItems().map(set -> item.is(set::contains)).orElse(true);
    }
    public boolean isSupportedItem(ItemStack item) {
        return item.is(definition.supportedItems()::contains);
    }
    public int getWeight() {
        return definition.weight();
    }
    public int getAnvilCost() {
        return definition.anvilCost();
    }
    public int getMinLevel() {
        return 1;
    }
    public int getMaxLevel() {
        return definition.maxLevel();
    }
    public int getMinCost(int level) {
        return definition.minCost().calculate(level);
    }
    public int getMaxCost(int level) {
        return definition.maxCost().calculate(level);
    }
    public String toString() {
        return "Enchantment " + description.getString();
    }
    public static boolean areCompatible(Holder<Enchantment> enchantment, Holder<Enchantment> other) {
        return !enchantment.value().exclusiveSet().contains(other) && !other.value().exclusiveSet().contains(enchantment);
    }
    public static Component getFullname(Holder<Enchantment> enchantment, int level) {
        Enchantment value = enchantment.value();
        net.minecraft.network.chat.MutableComponent name = value.description().copy();
        name.withStyle(enchantment.unwrapKey().map(key -> org.basaltmc.bukkit.item.TagSets.members("enchantment", "minecraft:curse").contains(key.identifier().toString())).orElse(false) ? net.minecraft.ChatFormatting.RED : net.minecraft.ChatFormatting.GRAY);
        if (level != 1 || value.getMaxLevel() != 1) {
            name.append(Component.literal(" ")).append(Component.translatable("enchantment.level." + level));
        }
        return name;
    }
    public boolean canEnchant(ItemStack itemStack) {
        return isSupportedItem(itemStack);
    }
    public <T> List<T> getEffects(DataComponentType<List<T>> type) {
        List<ConditionalEffect<EnchantmentValueEffect>> typed = org.basaltmc.bukkit.facade.EnchantmentEffects.valueEffects(this, String.valueOf(type));
        if (typed == null) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment#getEffects(" + type + ")");
        }
        return (List<T>) (List<?>) typed;
    }
    public boolean isImmuneToDamage(ServerLevel serverLevel, int enchantmentLevel, Entity victim, DamageSource source) {
        return org.basaltmc.bukkit.facade.EnchantmentEffects.anyMatches(this, "minecraft:damage_immunity", victim, source);
    }
    public void modifyDamageProtection(ServerLevel serverLevel, int enchantmentLevel, ItemStack item, Entity victim, DamageSource source, MutableFloat protection) {
        protection.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:damage_protection", enchantmentLevel, serverLevel.getRandom(), protection.floatValue(), victim, source, item));
    }
    public void modifyDurabilityChange(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, MutableFloat change) {
        change.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:item_damage", enchantmentLevel, serverLevel.getRandom(), change.floatValue(), null, null, itemStack));
    }
    public void modifyAmmoCount(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, MutableFloat change) {
        change.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:ammo_use", enchantmentLevel, serverLevel.getRandom(), change.floatValue(), null, null, itemStack));
    }
    public void modifyPiercingCount(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, MutableFloat count) {
        count.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:projectile_piercing", enchantmentLevel, serverLevel.getRandom(), count.floatValue(), null, null, itemStack));
    }
    public void modifyBlockExperience(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, MutableFloat count) {
        count.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:block_experience", enchantmentLevel, serverLevel.getRandom(), count.floatValue(), null, null, itemStack));
    }
    public void modifyMobExperience(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, Entity killer, MutableFloat experience) {
        experience.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:mob_experience", enchantmentLevel, serverLevel.getRandom(), experience.floatValue(), null, null, itemStack));
    }
    public void modifyDurabilityToRepairFromXp(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, MutableFloat change) {
        change.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:repair_with_xp", enchantmentLevel, serverLevel.getRandom(), change.floatValue(), null, null, itemStack));
    }
    public void modifyTridentReturnToOwnerAcceleration(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, Entity trident, MutableFloat count) {
        count.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:trident_return_acceleration", enchantmentLevel, serverLevel.getRandom(), count.floatValue(), null, null, itemStack));
    }
    public void modifyTridentSpinAttackStrength(RandomSource random, int enchantmentLevel, MutableFloat strength) {
        strength.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:trident_spin_attack_strength", enchantmentLevel, random, strength.floatValue(), null, null, null));
    }
    public void modifyFishingTimeReduction(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, Entity fisher, MutableFloat timeReduction) {
        timeReduction.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:fishing_time_reduction", enchantmentLevel, serverLevel.getRandom(), timeReduction.floatValue(), null, null, itemStack));
    }
    public void modifyFishingLuckBonus(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, Entity fisher, MutableFloat luck) {
        luck.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:fishing_luck_bonus", enchantmentLevel, serverLevel.getRandom(), luck.floatValue(), null, null, itemStack));
    }
    public void modifyDamage(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, Entity victim, DamageSource damageSource, MutableFloat amount) {
        amount.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:damage", enchantmentLevel, serverLevel.getRandom(), amount.floatValue(), victim, damageSource, itemStack));
    }
    public void modifyFallBasedDamage(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, Entity victim, DamageSource damageSource, MutableFloat amount) {
        amount.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:smash_damage_per_fallen_block", enchantmentLevel, serverLevel.getRandom(), amount.floatValue(), victim, damageSource, itemStack));
    }
    public void modifyKnockback(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, Entity victim, DamageSource damageSource, MutableFloat amount) {
        amount.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:knockback", enchantmentLevel, serverLevel.getRandom(), amount.floatValue(), victim, damageSource, itemStack));
    }
    public void modifyArmorEffectivness(ServerLevel serverLevel, int enchantmentLevel, ItemStack itemStack, Entity victim, DamageSource damageSource, MutableFloat amount) {
        amount.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:armor_effectiveness", enchantmentLevel, serverLevel.getRandom(), amount.floatValue(), victim, damageSource, itemStack));
    }
    public void doPostAttack(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, EnchantmentTarget forTarget, Entity victim, DamageSource damageSource) {
        org.basaltmc.bukkit.facade.EnchantmentEffects.runEntity(this, "minecraft:post_attack", enchantmentLevel, serverLevel, victim, damageSource, item.owner());
    }
    public static void doPostAttack(TargetedConditionalEffect<EnchantmentEntityEffect> effect, ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity victim, DamageSource damageSource) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment#doPostAttack");
    }
    public void doPostPiercingAttack(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity user) {
        org.basaltmc.bukkit.facade.EnchantmentEffects.runEntity(this, "minecraft:post_piercing_attack", enchantmentLevel, serverLevel, user, null, user);
    }
    public void modifyProjectileCount(ServerLevel serverLevel, int enchantmentLevel, ItemStack weapon, Entity shooter, MutableFloat count) {
        count.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:projectile_count", enchantmentLevel, serverLevel.getRandom(), count.floatValue(), null, null, weapon));
    }
    public void modifyProjectileSpread(ServerLevel serverLevel, int enchantmentLevel, ItemStack weapon, Entity shooter, MutableFloat angle) {
        angle.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:projectile_spread", enchantmentLevel, serverLevel.getRandom(), angle.floatValue(), null, null, weapon));
    }
    public void modifyCrossbowChargeTime(RandomSource random, int enchantmentLevel, MutableFloat time) {
        time.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, "minecraft:crossbow_charge_time", enchantmentLevel, random, time.floatValue(), null, null, null));
    }
    public void modifyUnfilteredValue(DataComponentType<EnchantmentValueEffect> component, RandomSource random, int enchantmentLevel, MutableFloat value) {
        value.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(this, String.valueOf(component), enchantmentLevel, random, value.floatValue(), null, null, null));
    }
    public void tick(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity) {
        org.basaltmc.bukkit.facade.EnchantmentEffects.runEntity(this, "minecraft:tick", enchantmentLevel, serverLevel, entity, null, entity);
    }
    public void onProjectileSpawned(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse weapon, Entity projectile) {
        org.basaltmc.bukkit.facade.EnchantmentEffects.runEntity(this, "minecraft:projectile_spawned", enchantmentLevel, serverLevel, projectile, null, projectile);
    }
    public void onHitBlock(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse weapon, Entity projectile, Vec3 position, BlockState hitBlock) {
        org.basaltmc.bukkit.facade.EnchantmentEffects.runEntity(this, "minecraft:hit_block", enchantmentLevel, serverLevel, projectile, null, projectile);
    }
    public static LootContext damageContext(ServerLevel serverLevel, int enchantmentLevel, Entity victim, DamageSource source) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment#damageContext");
    }
    public void runLocationChangedEffects(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, LivingEntity entity) {
        org.basaltmc.bukkit.facade.EnchantmentEffects.runEntity(this, "minecraft:location_changed", enchantmentLevel, serverLevel, entity, null, entity);
    }
    public void stopLocationBasedEffects(int enchantmentLevel, EnchantedItemInUse item, LivingEntity entity) {
        
    }
    public static Builder enchantment(EnchantmentDefinition definition) {
        return new Builder(definition);
    }
    public static record Cost(int base, int perLevelAboveFirst) {
        public static final Codec<Cost> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.Enchantment.Cost.class, "CODEC");
        public int calculate(int level) {
            return base + perLevelAboveFirst * (level - 1);
        }
    }
    public static record EnchantmentDefinition(HolderSet<Item> supportedItems, Optional<HolderSet<Item>> primaryItems, int weight, int maxLevel, Cost minCost, Cost maxCost, int anvilCost, List<EquipmentSlotGroup> slots) {
        public static final MapCodec<EnchantmentDefinition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.Enchantment.EnchantmentDefinition.class, "CODEC");
    }
    public static class Builder {
        private final EnchantmentDefinition definition;
        private HolderSet<Enchantment> exclusiveSet = HolderSet.empty();
        public Builder(EnchantmentDefinition definition) {
            this.definition = definition;
        }
        public Builder exclusiveWith(HolderSet<Enchantment> set) {
            this.exclusiveSet = set;
            return this;
        }
        public <E> Builder withEffect(DataComponentType<List<ConditionalEffect<E>>> type, E effect, LootItemCondition.Builder condition) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment.Builder#withEffect");
        }
        public <E> Builder withEffect(DataComponentType<List<ConditionalEffect<E>>> type, E effect) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment.Builder#withEffect");
        }
        public <E> Builder withEffect(DataComponentType<List<TargetedConditionalEffect<E>>> type, EnchantmentTarget enchanted, EnchantmentTarget affected, E effect, LootItemCondition.Builder condition) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment.Builder#withEffect");
        }
        public <E> Builder withEffect(DataComponentType<List<TargetedConditionalEffect<E>>> type, EnchantmentTarget enchanted, EnchantmentTarget affected, E effect) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment.Builder#withEffect");
        }
        public Builder withEffect(DataComponentType<List<EnchantmentAttributeEffect>> type, EnchantmentAttributeEffect effect) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment.Builder#withEffect");
        }
        public <E> Builder withSpecialEffect(DataComponentType<E> type, E effect) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment.Builder#withSpecialEffect");
        }
        public Builder withEffect(DataComponentType<Unit> type) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment.Builder#withEffect");
        }
        public Enchantment build(Identifier descriptionKey) {
            return new Enchantment(Component.translatable("enchantment." + descriptionKey.getNamespace() + "." + descriptionKey.getPath().replace('/', '.')), definition, exclusiveSet, DataComponentMap.EMPTY);
        }
    }
    interface FloatAction<T> {
        float apply(T effect, float value);
        default GenericAction<T> asGeneric(MutableFloat v) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.enchantment.Enchantment.FloatAction#asGeneric");
        }
    }
    interface GenericAction<T> {
        void apply(T effect);
    }
}

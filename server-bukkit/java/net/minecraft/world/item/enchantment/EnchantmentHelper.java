// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import net.minecraft.util.Mth;
import org.apache.commons.lang3.mutable.MutableFloat;
import com.mojang.datafixers.util.Pair;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.item.ItemInstance;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.providers.EnchantmentProvider;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;
public class EnchantmentHelper {
    public static int getItemEnchantmentLevel(Holder<Enchantment> enchantment, ItemInstance piece) {
        return piece.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).getLevel(enchantment);
    }
    public static ItemEnchantments updateEnchantments(ItemStack itemStack, Consumer<ItemEnchantments.Mutable> consumer) {
        net.minecraft.core.component.DataComponentType<ItemEnchantments> type = enchantmentComponent(itemStack);
        ItemEnchantments current = itemStack.get(type);
        if (current == null) {
            return ItemEnchantments.EMPTY;
        }
        ItemEnchantments.Mutable mutable = new ItemEnchantments.Mutable(current);
        consumer.accept(mutable);
        ItemEnchantments updated = mutable.toImmutable();
        itemStack.set(type, updated);
        return updated;
    }
    public static boolean canStoreEnchantments(ItemStack itemStack) {
        return itemStack.has(enchantmentComponent(itemStack));
    }
    public static void setEnchantments(ItemStack itemStack, ItemEnchantments enchantments) {
        itemStack.set(enchantmentComponent(itemStack), enchantments);
    }
    public static ItemEnchantments getEnchantmentsForCrafting(ItemStack itemStack) {
        return itemStack.getOrDefault(enchantmentComponent(itemStack), ItemEnchantments.EMPTY);
    }
    public static boolean hasAnyEnchantments(ItemStack itemStack) {
        return !itemStack.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).isEmpty()
            || !itemStack.getOrDefault(net.minecraft.core.component.DataComponents.STORED_ENCHANTMENTS, ItemEnchantments.EMPTY).isEmpty();
    }
    public static int processDurabilityChange(ServerLevel serverLevel, ItemStack itemStack, int amount) {
        MutableFloat value = new MutableFloat(amount);
        forEachOnItem(itemStack, (enchantment, level) -> enchantment.value().modifyDurabilityChange(serverLevel, level, itemStack, value));
        return Math.max(0, value.intValue());
    }
    public static int processAmmoUse(ServerLevel serverLevel, ItemStack weapon, ItemStack ammo, int amount) {
        MutableFloat value = new MutableFloat(amount);
        forEachOnItem(weapon, (enchantment, level) -> enchantment.value().modifyAmmoCount(serverLevel, level, ammo, value));
        return Math.max(0, value.intValue());
    }
    public static int processBlockExperience(ServerLevel serverLevel, ItemStack itemStack, int amount) {
        MutableFloat value = new MutableFloat(amount);
        forEachOnItem(itemStack, (enchantment, level) -> enchantment.value().modifyBlockExperience(serverLevel, level, itemStack, value));
        return Math.max(0, value.intValue());
    }
    public static int processMobExperience(ServerLevel serverLevel, Entity killer, Entity killed, int amount) {
        MutableFloat value = new MutableFloat(amount);
        if (killer instanceof LivingEntity living) {
            forEachEquipped(living, (enchantment, level, item) -> enchantment.value().modifyMobExperience(serverLevel, level, item.itemStack(), killed, value));
        }
        return Math.max(0, value.intValue());
    }
    public static ItemStack createBook(EnchantmentInstance enchant) {
        ItemStack book = new ItemStack(net.minecraft.world.item.Items.ENCHANTED_BOOK);
        book.enchant(enchant.enchantment(), enchant.level());
        return book;
    }
    public static boolean isImmuneToDamage(ServerLevel serverLevel, LivingEntity victim, DamageSource source) {
        boolean[] immune = {false};
        forEachEquipped(victim, (enchantment, level, item) -> immune[0] |= enchantment.value().isImmuneToDamage(serverLevel, level, victim, source));
        return immune[0];
    }
    public static float getDamageProtection(ServerLevel serverLevel, LivingEntity victim, DamageSource source) {
        MutableFloat value = new MutableFloat(0.0F);
        forEachEquipped(victim, (enchantment, level, item) -> enchantment.value().modifyDamageProtection(serverLevel, level, item.itemStack(), victim, source, value));
        return value.floatValue();
    }
    public static float modifyDamage(ServerLevel serverLevel, ItemStack itemStack, Entity victim, DamageSource damageSource, float damage) {
        MutableFloat value = new MutableFloat(damage);
        forEachOnItem(itemStack, (enchantment, level) -> enchantment.value().modifyDamage(serverLevel, level, itemStack, victim, damageSource, value));
        return value.floatValue();
    }
    public static float modifyFallBasedDamage(ServerLevel serverLevel, ItemStack itemStack, Entity victim, DamageSource damageSource, float damage) {
        MutableFloat value = new MutableFloat(damage);
        forEachOnItem(itemStack, (enchantment, level) -> enchantment.value().modifyFallBasedDamage(serverLevel, level, itemStack, victim, damageSource, value));
        return value.floatValue();
    }
    public static float modifyArmorEffectiveness(ServerLevel serverLevel, ItemStack itemStack, Entity victim, DamageSource damageSource, float armorFraction) {
        MutableFloat value = new MutableFloat(armorFraction);
        forEachOnItem(itemStack, (enchantment, level) -> enchantment.value().modifyArmorEffectivness(serverLevel, level, itemStack, victim, damageSource, value));
        return Mth.clamp(value.floatValue(), 0.0F, 1.0F);
    }
    public static float modifyKnockback(ServerLevel serverLevel, ItemStack itemStack, Entity victim, DamageSource damageSource, float knockback) {
        MutableFloat value = new MutableFloat(knockback);
        forEachOnItem(itemStack, (enchantment, level) -> enchantment.value().modifyKnockback(serverLevel, level, itemStack, victim, damageSource, value));
        return value.floatValue();
    }
    public static void doPostAttackEffects(ServerLevel serverLevel, Entity victim, DamageSource damageSource) {
        if (victim instanceof LivingEntity living) {
            forEachEquipped(living, (enchantment, level, item) -> enchantment.value().doPostAttack(serverLevel, level, item, EnchantmentTarget.VICTIM, victim, damageSource));
        }
        if (damageSource.getEntity() instanceof LivingEntity attacker) {
            forEachEquipped(attacker, (enchantment, level, item) -> enchantment.value().doPostAttack(serverLevel, level, item, EnchantmentTarget.ATTACKER, victim, damageSource));
        }
    }
    public static void doPostPiercingAttackEffects(ServerLevel serverLevel, LivingEntity user) {
        forEachEquipped(user, (enchantment, level, item) -> enchantment.value().doPostPiercingAttack(serverLevel, level, item, user));
    }
    public static void doPostAttackEffectsWithItemSource(ServerLevel serverLevel, Entity victim, DamageSource damageSource, ItemStack source) {
        doPostAttackEffectsWithItemSourceOnBreak(serverLevel, victim, damageSource, source, null);
    }
    public static void doPostAttackEffectsWithItemSourceOnBreak(ServerLevel serverLevel, Entity victim, DamageSource damageSource, ItemStack source, Consumer<ItemStack> attackerlessOnBreak) {
        if (damageSource.getEntity() instanceof LivingEntity attacker) {
            EquipmentSlot slot = attacker.getMainHandItem() == source ? EquipmentSlot.MAINHAND : EquipmentSlot.OFFHAND;
            EnchantedItemInUse used = new EnchantedItemInUse(source, slot, attacker, attackerlessOnBreak);
            forEachOnItem(source, (enchantment, level) -> enchantment.value().doPostAttack(serverLevel, level, used, EnchantmentTarget.ATTACKER, victim, damageSource));
        }
    }
    public static void runLocationChangedEffects(ServerLevel serverLevel, LivingEntity entity) {
        forEachEquipped(entity, (enchantment, level, item) -> enchantment.value().runLocationChangedEffects(serverLevel, level, item, entity));
    }
    public static void runLocationChangedEffects(ServerLevel serverLevel, ItemStack stack, LivingEntity entity, EquipmentSlot slot) {
        EnchantedItemInUse used = new EnchantedItemInUse(stack, slot, entity);
        forEachOnItem(stack, (enchantment, level) -> {
            if (enchantment.value().matchingSlot(slot)) {
                enchantment.value().runLocationChangedEffects(serverLevel, level, used, entity);
            }
        });
    }
    public static void stopLocationBasedEffects(LivingEntity entity) {
        forEachEquipped(entity, (enchantment, level, item) -> enchantment.value().stopLocationBasedEffects(level, item, entity));
    }
    public static void stopLocationBasedEffects(ItemStack stack, LivingEntity entity, EquipmentSlot slot) {
        EnchantedItemInUse used = new EnchantedItemInUse(stack, slot, entity);
        forEachOnItem(stack, (enchantment, level) -> {
            if (enchantment.value().matchingSlot(slot)) {
                enchantment.value().stopLocationBasedEffects(level, used, entity);
            }
        });
    }
    public static void tickEffects(ServerLevel serverLevel, LivingEntity entity) {
        forEachEquipped(entity, (enchantment, level, item) -> enchantment.value().tick(serverLevel, level, item, entity));
    }
    public static int getEnchantmentLevel(Holder<Enchantment> enchantment, LivingEntity entity) {
        int[] best = {0};
        forEachEquipped(entity, (held, level, item) -> {
            if (held.equals(enchantment)) {
                best[0] = Math.max(best[0], level);
            }
        });
        return best[0];
    }
    public static int processProjectileCount(ServerLevel serverLevel, ItemStack weapon, Entity shooter, int count) {
        MutableFloat value = new MutableFloat(count);
        forEachOnItem(weapon, (enchantment, level) -> enchantment.value().modifyProjectileCount(serverLevel, level, weapon, shooter, value));
        return Math.max(0, value.intValue());
    }
    public static float processProjectileSpread(ServerLevel serverLevel, ItemStack weapon, Entity shooter, float angle) {
        MutableFloat value = new MutableFloat(angle);
        forEachOnItem(weapon, (enchantment, level) -> enchantment.value().modifyProjectileSpread(serverLevel, level, weapon, shooter, value));
        return Math.max(0.0F, value.floatValue());
    }
    public static int getPiercingCount(ServerLevel serverLevel, ItemStack weapon, ItemStack ammo) {
        MutableFloat value = new MutableFloat(0.0F);
        forEachOnItem(weapon, (enchantment, level) -> enchantment.value().modifyPiercingCount(serverLevel, level, ammo, value));
        return Math.max(0, value.intValue());
    }
    public static void onProjectileSpawned(ServerLevel serverLevel, ItemStack weapon, Projectile projectileEntity, Consumer<ItemStack> onBreak) {
        net.minecraft.world.entity.Entity owner = projectileEntity.getOwner();
        EquipmentSlot slot = owner instanceof LivingEntity living && living.getMainHandItem() == weapon ? EquipmentSlot.MAINHAND : EquipmentSlot.OFFHAND;
        EnchantedItemInUse used = new EnchantedItemInUse(weapon, slot, owner instanceof LivingEntity living ? living : null, onBreak);
        forEachOnItem(weapon, (enchantment, level) -> enchantment.value().onProjectileSpawned(serverLevel, level, used, projectileEntity));
    }
    public static void onHitBlock(ServerLevel serverLevel, ItemStack weapon, LivingEntity owner, Entity entity, EquipmentSlot slot, Vec3 hitLocation, BlockState hitBlock, Consumer<ItemStack> onBreak) {
        EnchantedItemInUse used = new EnchantedItemInUse(weapon, slot, owner, onBreak);
        forEachOnItem(weapon, (enchantment, level) -> enchantment.value().onHitBlock(serverLevel, level, used, entity, hitLocation, hitBlock));
    }
    public static int modifyDurabilityToRepairFromXp(ServerLevel serverLevel, ItemStack item, int durability) {
        MutableFloat value = new MutableFloat(durability);
        forEachOnItem(item, (enchantment, level) -> enchantment.value().modifyDurabilityToRepairFromXp(serverLevel, level, item, value));
        return Math.max(0, value.intValue());
    }
    public static float processEquipmentDropChance(ServerLevel serverLevel, LivingEntity entity, DamageSource killingBlow, float chance) {
        net.minecraft.world.entity.Entity attacker = killingBlow.getEntity();
        MutableFloat value = new MutableFloat(chance);
        if (attacker != null) {
            forEachEquipped(entity, (enchantment, level, item) -> value.setValue(org.basaltmc.bukkit.facade.EnchantmentEffects.apply(enchantment.value(), "minecraft:equipment_drops", level, entity.getRandom(), value.floatValue(), attacker, killingBlow, item.itemStack())));
        }
        return value.floatValue();
    }
    public static void forEachModifier(ItemStack itemStack, EquipmentSlotGroup slot, BiConsumer<Holder<Attribute>, AttributeModifier> consumer) {
        for (Holder<Enchantment> enchantment : itemStack.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).keySet()) {
            if (enchantment.value().definition().slots().stream().anyMatch(group -> group == slot || group == EquipmentSlotGroup.ANY)) {
                int level = getItemEnchantmentLevel(enchantment, itemStack);
                org.basaltmc.bukkit.facade.EnchantmentEffects.attributes(enchantment.value(), level, slot.getSerializedName(), consumer);
            }
        }
    }
    public static void forEachModifier(ItemStack itemStack, EquipmentSlot slot, BiConsumer<Holder<Attribute>, AttributeModifier> consumer) {
        for (Holder<Enchantment> enchantment : itemStack.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).keySet()) {
            if (enchantment.value().matchingSlot(slot)) {
                int level = getItemEnchantmentLevel(enchantment, itemStack);
                org.basaltmc.bukkit.facade.EnchantmentEffects.attributes(enchantment.value(), level, slot.getName(), consumer);
            }
        }
    }
    public static int getFishingLuckBonus(ServerLevel serverLevel, ItemStack rod, Entity fisher) {
        MutableFloat value = new MutableFloat(0.0F);
        forEachOnItem(rod, (enchantment, level) -> enchantment.value().modifyFishingLuckBonus(serverLevel, level, rod, fisher, value));
        return Math.max(0, value.intValue());
    }
    public static float getFishingTimeReduction(ServerLevel serverLevel, ItemStack rod, Entity fisher) {
        MutableFloat value = new MutableFloat(0.0F);
        forEachOnItem(rod, (enchantment, level) -> enchantment.value().modifyFishingTimeReduction(serverLevel, level, rod, fisher, value));
        return Math.max(0.0F, value.floatValue());
    }
    public static int getTridentReturnToOwnerAcceleration(ServerLevel serverLevel, ItemStack weapon, Entity trident) {
        MutableFloat value = new MutableFloat(0.0F);
        forEachOnItem(weapon, (enchantment, level) -> enchantment.value().modifyTridentReturnToOwnerAcceleration(serverLevel, level, weapon, trident, value));
        return Math.max(0, value.intValue());
    }
    public static float modifyCrossbowChargingTime(ItemStack crossbow, LivingEntity holder, float time) {
        MutableFloat value = new MutableFloat(time);
        forEachOnItem(crossbow, (enchantment, level) -> enchantment.value().modifyCrossbowChargeTime(holder.getRandom(), level, value));
        return Math.max(0.0F, value.floatValue());
    }
    public static float getTridentSpinAttackStrength(ItemStack trident, LivingEntity holder) {
        MutableFloat value = new MutableFloat(0.0F);
        forEachOnItem(trident, (enchantment, level) -> enchantment.value().modifyTridentSpinAttackStrength(holder.getRandom(), level, value));
        return value.floatValue();
    }
    public static boolean hasTag(ItemStack item, TagKey<Enchantment> tag) {
        for (Holder<Enchantment> enchantment : item.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).keySet()) {
            if (enchantment.is(tag)) {
                return true;
            }
        }
        return false;
    }
    public static boolean hasEffect(ItemStack item, String effectKey) {
        for (Holder<Enchantment> enchantment : item.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).keySet()) {
            if (org.basaltmc.bukkit.facade.EnchantmentEffects.has(enchantment.value(), effectKey)) {
                return true;
            }
        }
        return false;
    }
    public static boolean has(ItemStack item, DataComponentType<?> effectType) {
        for (Holder<Enchantment> enchantment : item.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).keySet()) {
            if (org.basaltmc.bukkit.facade.EnchantmentEffects.has(enchantment.value(), String.valueOf(effectType))) {
                return true;
            }
        }
        return false;
    }
    public static <T> Optional<T> pickHighestLevel(ItemStack itemStack, DataComponentType<List<T>> componentType) {
        Holder<Enchantment> best = null;
        int bestLevel = 0;
        for (var entry : itemStack.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).entrySet()) {
            if (entry.getIntValue() > bestLevel && org.basaltmc.bukkit.facade.EnchantmentEffects.has(entry.getKey().value(), String.valueOf(componentType))) {
                best = entry.getKey();
                bestLevel = entry.getIntValue();
            }
        }
        if (best == null) {
            return Optional.empty();
        }
        List<T> effects = best.value().getEffects(componentType);
        return effects.isEmpty() ? Optional.empty() : Optional.of(effects.get(0));
    }
    public static <T> Pair<T, Integer> getHighestLevel(ItemStack item, DataComponentType<T> effectType) {
        Holder<Enchantment> best = null;
        int bestLevel = 0;
        for (var entry : item.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).entrySet()) {
            if (entry.getIntValue() > bestLevel && org.basaltmc.bukkit.facade.EnchantmentEffects.has(entry.getKey().value(), String.valueOf(effectType))) {
                best = entry.getKey();
                bestLevel = entry.getIntValue();
            }
        }
        return best == null ? null : Pair.of(null, bestLevel);
    }
    public static Optional<EnchantedItemInUse> getRandomItemWith(DataComponentType<?> componentType, LivingEntity source, Predicate<ItemStack> predicate) {
        List<EnchantedItemInUse> found = new java.util.ArrayList<>();
        for (EquipmentSlot slot : EquipmentSlot.VALUES) {
            ItemStack stack = source.getItemBySlot(slot);
            if (stack.isEmpty() || !predicate.test(stack)) {
                continue;
            }
            for (Holder<Enchantment> enchantment : stack.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).keySet()) {
                if (enchantment.value().matchingSlot(slot) && org.basaltmc.bukkit.facade.EnchantmentEffects.has(enchantment.value(), String.valueOf(componentType))) {
                    found.add(new EnchantedItemInUse(stack, slot, source));
                    break;
                }
            }
        }
        return found.isEmpty() ? Optional.empty() : Optional.of(found.get(source.getRandom().nextInt(found.size())));
    }
    public static int getEnchantmentCost(RandomSource random, int slot, int bookcases, ItemStack itemStack) {
        int enchantability = enchantability(itemStack);
        if (enchantability <= 0) {
            return 0;
        }
        int shelves = Math.min(bookcases, 15);
        int base = random.nextInt(8) + 1 + (shelves >> 1) + random.nextInt(shelves + 1);
        if (slot == 0) {
            return Math.max(base / 3, 1);
        }
        return slot == 1 ? base * 2 / 3 + 1 : Math.max(base, shelves * 2);
    }
    public static ItemStack enchantItem(RandomSource random, ItemStack itemStack, int enchantmentCost, RegistryAccess registryAccess, Optional<? extends HolderSet<Enchantment>> set) {
        Stream<Holder<Enchantment>> pool = set.isPresent() ? set.get().stream().map(h -> (Holder<Enchantment>) h) : registryAccess.lookupOrThrow(net.minecraft.core.registries.Registries.ENCHANTMENT).listElements().map(h -> (Holder<Enchantment>) (Holder) h);
        return enchantItem(random, itemStack, enchantmentCost, pool);
    }
    public static ItemStack enchantItem(RandomSource random, ItemStack itemStack, int enchantmentCost, Stream<Holder<Enchantment>> source) {
        List<EnchantmentInstance> chosen = selectEnchantment(random, itemStack, enchantmentCost, source);
        if (itemStack.is(net.minecraft.world.item.Items.BOOK)) {
            itemStack = itemStack.transmuteCopy(net.minecraft.world.item.Items.ENCHANTED_BOOK);
        }
        for (EnchantmentInstance instance : chosen) {
            itemStack.enchant(instance.enchantment(), instance.level());
        }
        return itemStack;
    }
    public static List<EnchantmentInstance> selectEnchantment(RandomSource random, ItemStack itemStack, int enchantmentCost, Stream<Holder<Enchantment>> source) {
        List<EnchantmentInstance> results = new java.util.ArrayList<>();
        int enchantability = enchantability(itemStack);
        if (enchantability <= 0) {
            return results;
        }
        int cost = enchantmentCost + 1 + random.nextInt(enchantability / 4 + 1) + random.nextInt(enchantability / 4 + 1);
        float bonus = (random.nextFloat() + random.nextFloat() - 1.0F) * 0.15F;
        cost = Mth.clamp(Math.round(cost + cost * bonus), 1, Integer.MAX_VALUE);
        List<EnchantmentInstance> available = getAvailableEnchantmentResults(cost, itemStack, source);
        if (available.isEmpty()) {
            return results;
        }
        results.add(pickWeighted(random, available));
        while (random.nextInt(50) <= cost) {
            filterCompatibleEnchantments(available, results.get(results.size() - 1));
            if (available.isEmpty()) {
                break;
            }
            results.add(pickWeighted(random, available));
            cost /= 2;
        }
        return results;
    }
    public static void filterCompatibleEnchantments(List<EnchantmentInstance> enchants, EnchantmentInstance target) {
        enchants.removeIf(candidate -> !Enchantment.areCompatible(target.enchantment(), candidate.enchantment()));
    }
    public static boolean isEnchantmentCompatible(Collection<Holder<Enchantment>> enchants, Holder<Enchantment> target) {
        for (Holder<Enchantment> existing : enchants) {
            if (!existing.equals(target) && !Enchantment.areCompatible(existing, target)) {
                return false;
            }
        }
        return true;
    }
    public static List<EnchantmentInstance> getAvailableEnchantmentResults(int value, ItemStack itemStack, Stream<Holder<Enchantment>> source) {
        List<EnchantmentInstance> out = new java.util.ArrayList<>();
        boolean book = itemStack.is(net.minecraft.world.item.Items.BOOK);
        source.filter(holder -> holder.value().isPrimaryItem(itemStack) || book).forEach(holder -> {
            Enchantment enchantment = holder.value();
            for (int level = enchantment.getMaxLevel(); level >= enchantment.getMinLevel(); level--) {
                if (value >= enchantment.getMinCost(level) && value <= enchantment.getMaxCost(level)) {
                    out.add(new EnchantmentInstance(holder, level));
                    break;
                }
            }
        });
        return out;
    }
    public static void enchantItemFromProvider(ItemStack itemStack, RegistryAccess registryAccess, ResourceKey<EnchantmentProvider> providerKey, DifficultyInstance difficulty, RandomSource random) {
        registryAccess.lookup(net.minecraft.core.registries.Registries.ENCHANTMENT_PROVIDER).flatMap(lookup -> lookup.get(providerKey)).ifPresent(provider -> {
            ItemEnchantments.Mutable mutable = new ItemEnchantments.Mutable(getEnchantmentsForCrafting(itemStack));
            provider.value().enchant(itemStack, mutable, random, difficulty);
            setEnchantments(itemStack, mutable.toImmutable());
        });
    }
    private static int enchantability(ItemStack stack) {
        Enchantable component = stack.get(net.minecraft.core.component.DataComponents.ENCHANTABLE);
        return component == null ? 0 : component.value();
    }
    private static void forEachOnItem(ItemStack stack, EnchantmentVisitor visitor) {
        for (var entry : stack.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).entrySet()) {
            visitor.accept(entry.getKey(), entry.getIntValue());
        }
    }
    private static void forEachEquipped(LivingEntity entity, EnchantmentInSlotVisitor visitor) {
        for (EquipmentSlot slot : EquipmentSlot.VALUES) {
            ItemStack stack = entity.getItemBySlot(slot);
            if (stack.isEmpty()) {
                continue;
            }
            EnchantedItemInUse used = new EnchantedItemInUse(stack, slot, entity);
            for (var entry : stack.getOrDefault(net.minecraft.core.component.DataComponents.ENCHANTMENTS, ItemEnchantments.EMPTY).entrySet()) {
                if (entry.getKey().value().matchingSlot(slot)) {
                    visitor.accept(entry.getKey(), entry.getIntValue(), used);
                }
            }
        }
    }
    private static EnchantmentInstance pickWeighted(RandomSource random, List<EnchantmentInstance> pool) {
        int total = 0;
        for (EnchantmentInstance candidate : pool) {
            total += candidate.weight();
        }
        int roll = random.nextInt(total);
        for (EnchantmentInstance candidate : pool) {
            roll -= candidate.weight();
            if (roll < 0) {
                return candidate;
            }
        }
        return pool.get(pool.size() - 1);
    }
    interface EnchantmentInSlotVisitor {
        void accept(Holder<Enchantment> enchantment, int level, EnchantedItemInUse item);
    }
    interface EnchantmentVisitor {
        void accept(Holder<Enchantment> enchantment, int level);
    }
    private static net.minecraft.core.component.DataComponentType<ItemEnchantments> enchantmentComponent(ItemStack itemStack) {
        return itemStack.is(net.minecraft.world.item.Items.ENCHANTED_BOOK)
            ? net.minecraft.core.component.DataComponents.STORED_ENCHANTMENTS : net.minecraft.core.component.DataComponents.ENCHANTMENTS;
    }
}

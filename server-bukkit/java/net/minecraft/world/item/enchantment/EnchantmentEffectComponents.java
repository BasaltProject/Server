// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import net.minecraft.core.registries.BuiltInRegistries;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.Unit;
import net.minecraft.world.item.CrossbowItem;
import net.minecraft.world.item.enchantment.effects.DamageImmunity;
import net.minecraft.world.item.enchantment.effects.EnchantmentAttributeEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentEntityEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentLocationBasedEffect;
import net.minecraft.world.item.enchantment.effects.EnchantmentValueEffect;
public interface EnchantmentEffectComponents {
    private static <T> DataComponentType<T> register(String name) {
        DataComponentType<T> type = DataComponentType.<T>builder().build(name);
        return Registry.register(BuiltInRegistries.ENCHANTMENT_EFFECT_COMPONENT_TYPE, name, type);
    }
    Codec<DataComponentType<?>> COMPONENT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.EnchantmentEffectComponents.class, "COMPONENT_CODEC");
    Codec<DataComponentMap> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.EnchantmentEffectComponents.class, "CODEC");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> DAMAGE_PROTECTION = register("damage_protection");
    DataComponentType<List<ConditionalEffect<DamageImmunity>>> DAMAGE_IMMUNITY = register("damage_immunity");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> DAMAGE = register("damage");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> SMASH_DAMAGE_PER_FALLEN_BLOCK = register("smash_damage_per_fallen_block");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> KNOCKBACK = register("knockback");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> ARMOR_EFFECTIVENESS = register("armor_effectiveness");
    DataComponentType<List<TargetedConditionalEffect<EnchantmentEntityEffect>>> POST_ATTACK = register("post_attack");
    DataComponentType<List<ConditionalEffect<EnchantmentEntityEffect>>> POST_PIERCING_ATTACK = register("post_piercing_attack");
    DataComponentType<List<ConditionalEffect<EnchantmentEntityEffect>>> HIT_BLOCK = register("hit_block");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> ITEM_DAMAGE = register("item_damage");
    DataComponentType<List<TargetedConditionalEffect<EnchantmentValueEffect>>> EQUIPMENT_DROPS = register("equipment_drops");
    DataComponentType<List<ConditionalEffect<EnchantmentLocationBasedEffect>>> LOCATION_CHANGED = register("location_changed");
    DataComponentType<List<ConditionalEffect<EnchantmentEntityEffect>>> TICK = register("tick");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> AMMO_USE = register("ammo_use");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> PROJECTILE_PIERCING = register("projectile_piercing");
    DataComponentType<List<ConditionalEffect<EnchantmentEntityEffect>>> PROJECTILE_SPAWNED = register("projectile_spawned");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> PROJECTILE_SPREAD = register("projectile_spread");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> PROJECTILE_COUNT = register("projectile_count");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> TRIDENT_RETURN_ACCELERATION = register("trident_return_acceleration");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> FISHING_TIME_REDUCTION = register("fishing_time_reduction");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> FISHING_LUCK_BONUS = register("fishing_luck_bonus");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> BLOCK_EXPERIENCE = register("block_experience");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> MOB_EXPERIENCE = register("mob_experience");
    DataComponentType<List<ConditionalEffect<EnchantmentValueEffect>>> REPAIR_WITH_XP = register("repair_with_xp");
    DataComponentType<List<EnchantmentAttributeEffect>> ATTRIBUTES = register("attributes");
    DataComponentType<EnchantmentValueEffect> CROSSBOW_CHARGE_TIME = register("crossbow_charge_time");
    DataComponentType<List<CrossbowItem.ChargingSounds>> CROSSBOW_CHARGING_SOUNDS = register("crossbow_charging_sounds");
    DataComponentType<List<Holder<SoundEvent>>> TRIDENT_SOUND = register("trident_sound");
    DataComponentType<Unit> PREVENT_EQUIPMENT_DROP = register("prevent_equipment_drop");
    DataComponentType<Unit> PREVENT_ARMOR_CHANGE = register("prevent_armor_change");
    DataComponentType<EnchantmentValueEffect> TRIDENT_SPIN_ATTACK_STRENGTH = register("trident_spin_attack_strength");
    static DataComponentType<?> bootstrap(Registry<DataComponentType<?>> registry) {
        return DAMAGE_PROTECTION;

    }
}

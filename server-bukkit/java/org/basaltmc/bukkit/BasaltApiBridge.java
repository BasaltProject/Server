package org.basaltmc.bukkit;

import io.papermc.paper.plugin.lifecycle.event.LifecycleEventManager;
import java.util.function.BooleanSupplier;
import net.kyori.adventure.text.flattener.ComponentFlattener;
import org.bukkit.plugin.Plugin;
import org.bukkit.plugin.java.JavaPlugin;
import org.basaltmc.bukkit.gen.InternalAPIBridgeBase;

/** The hooks Paper's API reaches the server through; what is not here reports itself once when used. */
public final class BasaltApiBridge extends InternalAPIBridgeBase {
    @Override
    public LifecycleEventManager<Plugin> createPluginLifecycleEventManager(JavaPlugin plugin,
            BooleanSupplier registrationCheck) {
        return new org.basaltmc.bukkit.brigadier.BasaltLifecycleManager(plugin);
    }

    @Override
    public <MODERN, LEGACY> org.bukkit.GameRule<LEGACY> legacyGameRuleBridge(org.bukkit.GameRule<MODERN> modern,
        java.util.function.Function<LEGACY, MODERN> toModern, java.util.function.Function<MODERN, LEGACY> fromModern,
        Class<LEGACY> type) {
        return new BasaltGameRule.Legacy<>(modern, toModern, fromModern, type);
    }

    @Override
    public io.papermc.paper.entity.poi.PoiType.Occupancy createOccupancy(String name) {
        return new io.papermc.paper.entity.poi.PoiType.Occupancy() {
            @Override
            public String toString() {
                return name;
            }
        };
    }

    @Override
    public org.bukkit.block.Biome constructLegacyCustomBiome() {
        return new BasaltBiome("minecraft:custom", -1);
    }

    @Override
    public org.bukkit.inventory.ItemStack createEmptyStack() {
        return new BasaltItemStack(org.bukkit.Material.AIR, 0);
    }

    @Override
    public org.bukkit.inventory.ItemStack deserializeItem(byte[] bytes) {
        return BasaltItemStack.restore(bytes);
    }

    @Override
    public org.bukkit.damage.DamageSource.Builder createDamageSourceBuilder(org.bukkit.damage.DamageType type) {
        return new BasaltDamageSource.Builder(type);
    }

    @Override
    public java.util.function.Predicate<io.papermc.paper.command.brigadier.CommandSourceStack> restricted(
            java.util.function.Predicate<io.papermc.paper.command.brigadier.CommandSourceStack> predicate,
            boolean restricted) {
        return predicate;
    }

    @Override
    public ComponentFlattener componentFlattener() {
        return ComponentFlattener.basic();
    }

    private static final java.util.Map<String, String> kEffectSounds = java.util.Map.of("hurt", "entity.player.hurt",
        "thorns", "enchant.thorns.hit", "drowning", "entity.player.hurt_drown", "burning", "entity.player.hurt_on_fire",
        "poking", "block.sweet_berry_bush.pick_berries", "freezing", "entity.player.hurt_freeze");

    static org.bukkit.damage.DamageEffect damageEffect(String name) {
        String sound = kEffectSounds.getOrDefault(name, kEffectSounds.get("hurt"));
        return () -> org.bukkit.Registry.SOUNDS.get(org.bukkit.NamespacedKey.minecraft(sound));
    }

    @Override
    public org.bukkit.damage.DamageEffect getDamageEffect(String id) {
        return damageEffect(id);
    }

    @Override
    public <C> io.papermc.paper.persistence.PersistentDataKey<C> createPersistentDataKey(net.kyori.adventure.key.Key key,
            org.bukkit.persistence.PersistentDataType<?, C> type) {
        org.bukkit.NamespacedKey own = new org.bukkit.NamespacedKey(key.namespace(), key.value());
        return new io.papermc.paper.persistence.PersistentDataKey<>() {
            @Override
            public org.bukkit.NamespacedKey key() {
                return own;
            }

            @Override
            public org.bukkit.persistence.PersistentDataType<?, C> dataType() {
                return type;
            }
        };
    }

    @Override
    public String getTranslationKey(org.bukkit.entity.EntityType type) {
        return "entity." + type.getKey().getNamespace() + "." + type.getKey().getKey();
    }

    @Override
    public org.bukkit.entity.SpawnCategory getSpawnCategory(org.bukkit.entity.EntityType type) {
        return switch (EntityCategories.of(type.getKey().getKey())) {
            case "monster" -> org.bukkit.entity.SpawnCategory.MONSTER;
            case "creature" -> org.bukkit.entity.SpawnCategory.ANIMAL;
            case "ambient" -> org.bukkit.entity.SpawnCategory.AMBIENT;
            case "axolotls" -> org.bukkit.entity.SpawnCategory.AXOLOTL;
            case "underground_water_creature" -> org.bukkit.entity.SpawnCategory.WATER_UNDERGROUND_CREATURE;
            case "water_creature" -> org.bukkit.entity.SpawnCategory.WATER_ANIMAL;
            case "water_ambient" -> org.bukkit.entity.SpawnCategory.WATER_AMBIENT;
            default -> org.bukkit.entity.SpawnCategory.MISC;
        };
    }

    @Override
    public boolean hasDefaultEntityAttributes(org.bukkit.NamespacedKey type) {
        return LeftoverNatives.entityDefaults(type.toString()).length > 0;
    }

    @Override
    public org.bukkit.attribute.Attributable getDefaultEntityAttributes(org.bukkit.NamespacedKey type) {
        String[] defaults = LeftoverNatives.entityDefaults(type.toString());
        if (defaults.length == 0) {
            throw new IllegalArgumentException("The entity type " + type + " has no attributes");
        }
        return new DefaultAttributes(defaults);
    }

    @Override
    public String getStatisticCriteriaKey(org.bukkit.Statistic statistic) {
        if (statistic.getType() == org.bukkit.Statistic.Type.UNTYPED) {
            return org.basaltmc.bukkit.player.PlayerCalls.statisticCriteria(statistic);
        }
        return "minecraft." + switch (statistic) {
            case MINE_BLOCK -> "mined";
            case USE_ITEM -> "used";
            case BREAK_ITEM -> "broken";
            case CRAFT_ITEM -> "crafted";
            case PICKUP -> "picked_up";
            case DROP -> "dropped";
            case KILL_ENTITY -> "killed";
            default -> "killed_by";
        };
    }

    @Override
    public io.papermc.paper.world.damagesource.CombatEntry createCombatEntry(org.bukkit.entity.LivingEntity entity,
            org.bukkit.damage.DamageSource source, float damage) {
        return new BasaltCombatEntry(source, damage, io.papermc.paper.world.damagesource.FallLocationType.GENERIC,
            entity.getFallDistance());
    }

    @Override
    public io.papermc.paper.world.damagesource.CombatEntry createCombatEntry(org.bukkit.damage.DamageSource source,
            float damage, io.papermc.paper.world.damagesource.FallLocationType location, float fallDistance) {
        return new BasaltCombatEntry(source, damage, location, fallDistance);
    }

    @Override
    public io.papermc.paper.datacomponent.item.ResolvableProfile defaultMannequinProfile() {
        return (io.papermc.paper.datacomponent.item.ResolvableProfile) org.basaltmc.bukkit.datacomponent.ContentSpecs
            .PROFILE.value(new java.util.LinkedHashMap<>());
    }

    @Override
    public net.kyori.adventure.text.Component defaultMannequinDescription() {
        return net.kyori.adventure.text.Component.translatable("entity.minecraft.mannequin.label");
    }

    @Override
    public java.util.Set<org.bukkit.entity.Pose> validMannequinPoses() {
        return java.util.EnumSet.of(org.bukkit.entity.Pose.STANDING, org.bukkit.entity.Pose.SNEAKING,
            org.bukkit.entity.Pose.SWIMMING, org.bukkit.entity.Pose.FALL_FLYING, org.bukkit.entity.Pose.SLEEPING);
    }

    @Override
    public com.destroystokyo.paper.SkinParts.Mutable allSkinParts() {
        return new org.basaltmc.bukkit.misc.BasaltSkinParts(0x7F);
    }
}

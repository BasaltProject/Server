// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EntityType;

public interface EntityTypeTags {
    TagKey<EntityType<?>> SKELETONS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "skeletons");
    TagKey<EntityType<?>> ZOMBIES = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "zombies");
    TagKey<EntityType<?>> RAIDERS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "raiders");
    TagKey<EntityType<?>> UNDEAD = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "undead");
    TagKey<EntityType<?>> BURN_IN_DAYLIGHT = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "burn_in_daylight");
    TagKey<EntityType<?>> BEEHIVE_INHABITORS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "beehive_inhabitors");
    TagKey<EntityType<?>> ARROWS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "arrows");
    TagKey<EntityType<?>> IMPACT_PROJECTILES = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "impact_projectiles");
    TagKey<EntityType<?>> POWDER_SNOW_WALKABLE_MOBS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "powder_snow_walkable_mobs");
    TagKey<EntityType<?>> AXOLOTL_ALWAYS_HOSTILES = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "axolotl_always_hostiles");
    TagKey<EntityType<?>> AXOLOTL_HUNT_TARGETS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "axolotl_hunt_targets");
    TagKey<EntityType<?>> FREEZE_IMMUNE_ENTITY_TYPES = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "freeze_immune_entity_types");
    TagKey<EntityType<?>> FREEZE_HURTS_EXTRA_TYPES = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "freeze_hurts_extra_types");
    TagKey<EntityType<?>> CAN_BREATHE_UNDER_WATER = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "can_breathe_under_water");
    TagKey<EntityType<?>> FROG_FOOD = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "frog_food");
    TagKey<EntityType<?>> FALL_DAMAGE_IMMUNE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "fall_damage_immune");
    TagKey<EntityType<?>> DISMOUNTS_UNDERWATER = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "dismounts_underwater");
    TagKey<EntityType<?>> NON_CONTROLLING_RIDER = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "non_controlling_rider");
    TagKey<EntityType<?>> DEFLECTS_PROJECTILES = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "deflects_projectiles");
    TagKey<EntityType<?>> CAN_TURN_IN_BOATS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "can_turn_in_boats");
    TagKey<EntityType<?>> ILLAGER = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "illager");
    TagKey<EntityType<?>> AQUATIC = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "aquatic");
    TagKey<EntityType<?>> ARTHROPOD = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "arthropod");
    TagKey<EntityType<?>> IGNORES_POISON_AND_REGEN = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "ignores_poison_and_regen");
    TagKey<EntityType<?>> INVERTED_HEALING_AND_HARM = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "inverted_healing_and_harm");
    TagKey<EntityType<?>> WITHER_FRIENDS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "wither_friends");
    TagKey<EntityType<?>> ILLAGER_FRIENDS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "illager_friends");
    TagKey<EntityType<?>> NOT_SCARY_FOR_PUFFERFISH = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "not_scary_for_pufferfish");
    TagKey<EntityType<?>> SENSITIVE_TO_IMPALING = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "sensitive_to_impaling");
    TagKey<EntityType<?>> SENSITIVE_TO_BANE_OF_ARTHROPODS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "sensitive_to_bane_of_arthropods");
    TagKey<EntityType<?>> SENSITIVE_TO_SMITE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "sensitive_to_smite");
    TagKey<EntityType<?>> NO_ANGER_FROM_WIND_CHARGE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "no_anger_from_wind_charge");
    TagKey<EntityType<?>> IMMUNE_TO_OOZING = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "immune_to_oozing");
    TagKey<EntityType<?>> IMMUNE_TO_INFESTED = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "immune_to_infested");
    TagKey<EntityType<?>> REDIRECTABLE_PROJECTILE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "redirectable_projectile");
    TagKey<EntityType<?>> BOAT = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "boat");
    TagKey<EntityType<?>> CAN_EQUIP_SADDLE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "can_equip_saddle");
    TagKey<EntityType<?>> CAN_EQUIP_HARNESS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "can_equip_harness");
    TagKey<EntityType<?>> CAN_WEAR_HORSE_ARMOR = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "can_wear_horse_armor");
    TagKey<EntityType<?>> CAN_WEAR_NAUTILUS_ARMOR = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "can_wear_nautilus_armor");
    TagKey<EntityType<?>> FOLLOWABLE_FRIENDLY_MOBS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "followable_friendly_mobs");
    TagKey<EntityType<?>> CANNOT_BE_PUSHED_ONTO_BOATS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "cannot_be_pushed_onto_boats");
    TagKey<EntityType<?>> ACCEPTS_IRON_GOLEM_GIFT = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "accepts_iron_golem_gift");
    TagKey<EntityType<?>> CANDIDATE_FOR_IRON_GOLEM_GIFT = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "candidate_for_iron_golem_gift");
    TagKey<EntityType<?>> NAUTILUS_HOSTILES = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "nautilus_hostiles");
    TagKey<EntityType<?>> CAN_FLOAT_WHILE_RIDDEN = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "can_float_while_ridden");
    TagKey<EntityType<?>> CANNOT_BE_AGE_LOCKED = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "cannot_be_age_locked");
    TagKey<EntityType<?>> NOT_AFFECTED_BY_GEYSERS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "not_affected_by_geysers");
    TagKey<EntityType<?>> CANNOT_BE_DISMOUNTED_BY_ITEM_USAGE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENTITY_TYPE, "cannot_be_dismounted_by_item_usage");
}

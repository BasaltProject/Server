// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.damagesource.DamageType;

public interface DamageTypeTags {
    TagKey<DamageType> DAMAGES_HELMET = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "damages_helmet");
    TagKey<DamageType> BYPASSES_ARMOR = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "bypasses_armor");
    TagKey<DamageType> BYPASSES_SHIELD = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "bypasses_shield");
    TagKey<DamageType> BYPASSES_INVULNERABILITY = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "bypasses_invulnerability");
    TagKey<DamageType> BYPASSES_COOLDOWN = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "bypasses_cooldown");
    TagKey<DamageType> BYPASSES_EFFECTS = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "bypasses_effects");
    TagKey<DamageType> BYPASSES_RESISTANCE = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "bypasses_resistance");
    TagKey<DamageType> BYPASSES_ENCHANTMENTS = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "bypasses_enchantments");
    TagKey<DamageType> IS_FIRE = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "is_fire");
    TagKey<DamageType> IS_PROJECTILE = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "is_projectile");
    TagKey<DamageType> WITCH_RESISTANT_TO = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "witch_resistant_to");
    TagKey<DamageType> IS_EXPLOSION = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "is_explosion");
    TagKey<DamageType> IS_FALL = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "is_fall");
    TagKey<DamageType> IS_DROWNING = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "is_drowning");
    TagKey<DamageType> IS_FREEZING = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "is_freezing");
    TagKey<DamageType> IS_LIGHTNING = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "is_lightning");
    TagKey<DamageType> NO_ANGER = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "no_anger");
    TagKey<DamageType> NO_WOLF_RETALIATION = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "no_wolf_retaliation");
    TagKey<DamageType> NO_IMPACT = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "no_impact");
    TagKey<DamageType> ALWAYS_MOST_SIGNIFICANT_FALL = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "always_most_significant_fall");
    TagKey<DamageType> WITHER_IMMUNE_TO = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "wither_immune_to");
    TagKey<DamageType> SULFUR_CUBE_WITH_BLOCK_IMMUNE_TO = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "sulfur_cube_with_block_immune_to");
    TagKey<DamageType> IGNITES_ARMOR_STANDS = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "ignites_armor_stands");
    TagKey<DamageType> BURNS_ARMOR_STANDS = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "burns_armor_stands");
    TagKey<DamageType> AVOIDS_GUARDIAN_THORNS = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "avoids_guardian_thorns");
    TagKey<DamageType> ALWAYS_TRIGGERS_SILVERFISH = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "always_triggers_silverfish");
    TagKey<DamageType> ALWAYS_HURTS_ENDER_DRAGONS = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "always_hurts_ender_dragons");
    TagKey<DamageType> NO_KNOCKBACK = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "no_knockback");
    TagKey<DamageType> ALWAYS_KILLS_ARMOR_STANDS = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "always_kills_armor_stands");
    TagKey<DamageType> CAN_BREAK_ARMOR_STAND = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "can_break_armor_stand");
    TagKey<DamageType> BYPASSES_WOLF_ARMOR = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "bypasses_wolf_armor");
    TagKey<DamageType> IS_PLAYER_ATTACK = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "is_player_attack");
    TagKey<DamageType> BURN_FROM_STEPPING = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "burn_from_stepping");
    TagKey<DamageType> PANIC_CAUSES = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "panic_causes");
    TagKey<DamageType> PANIC_ENVIRONMENTAL_CAUSES = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "panic_environmental_causes");
    TagKey<DamageType> IS_MACE_SMASH = org.basaltmc.bukkit.facade.Tags.key(Registries.DAMAGE_TYPE, "mace_smash");
}

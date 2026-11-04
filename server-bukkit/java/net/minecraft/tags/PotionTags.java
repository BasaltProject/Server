// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.alchemy.Potion;

public final class PotionTags {
    public static final TagKey<Potion> TRADEABLE = org.basaltmc.bukkit.facade.Tags.key(Registries.POTION, "tradeable");
    public static final TagKey<Potion> DOUSES_FIRE = org.basaltmc.bukkit.facade.Tags.key(Registries.POTION, "douses_fire");
    public static final TagKey<Potion> HURTS_WATER_SENSITIVE_ENTITIES = org.basaltmc.bukkit.facade.Tags.key(Registries.POTION, "hurts_water_sensitive_entities");
    public static final TagKey<Potion> EXTINGUISHES_ENTITIES = org.basaltmc.bukkit.facade.Tags.key(Registries.POTION, "extinguishes_entities");
    public static final TagKey<Potion> REHYDRATES_AXOLOTLS = org.basaltmc.bukkit.facade.Tags.key(Registries.POTION, "rehydrates_axolotls");

    private PotionTags() {}
}

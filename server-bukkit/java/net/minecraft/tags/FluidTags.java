// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.material.Fluid;

public final class FluidTags {
    public static final TagKey<Fluid> WATER = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "water");
    public static final TagKey<Fluid> LAVA = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "lava");
    public static final TagKey<Fluid> SUPPORTS_SUGAR_CANE_ADJACENTLY = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "supports_sugar_cane_adjacently");
    public static final TagKey<Fluid> SUPPORTS_LILY_PAD = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "supports_lily_pad");
    public static final TagKey<Fluid> SUPPORTS_FROGSPAWN = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "supports_frogspawn");
    public static final TagKey<Fluid> BUBBLE_COLUMN_CAN_OCCUPY = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "bubble_column_can_occupy");
    public static final TagKey<Fluid> AXOLOTL_TRIES_TO_FIND = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "axolotl_tries_to_find");
    public static final TagKey<Fluid> DOLPHIN_TRIES_TO_FIND = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "dolphin_tries_to_find");
    public static final TagKey<Fluid> FROG_TRIES_TO_FIND_LAND_NEAR = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "frog_tries_to_find_land_near");
    public static final TagKey<Fluid> ENTITY_FLOATABLE = org.basaltmc.bukkit.facade.Tags.key(Registries.FLUID, "entity_floatable");

    private FluidTags() {}
}

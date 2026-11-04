// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.decoration.painting.PaintingVariant;

public final class PaintingVariantTags {
    public static final TagKey<PaintingVariant> PLACEABLE = org.basaltmc.bukkit.facade.Tags.key(Registries.PAINTING_VARIANT, "placeable");

    private PaintingVariantTags() {}
}

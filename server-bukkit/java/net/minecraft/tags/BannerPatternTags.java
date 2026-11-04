// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.entity.BannerPattern;

public final class BannerPatternTags {
    public static final TagKey<BannerPattern> NO_ITEM_REQUIRED = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "no_item_required");
    public static final TagKey<BannerPattern> PATTERN_ITEM_FLOWER = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/flower");
    public static final TagKey<BannerPattern> PATTERN_ITEM_CREEPER = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/creeper");
    public static final TagKey<BannerPattern> PATTERN_ITEM_SKULL = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/skull");
    public static final TagKey<BannerPattern> PATTERN_ITEM_MOJANG = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/mojang");
    public static final TagKey<BannerPattern> PATTERN_ITEM_GLOBE = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/globe");
    public static final TagKey<BannerPattern> PATTERN_ITEM_PIGLIN = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/piglin");
    public static final TagKey<BannerPattern> PATTERN_ITEM_FLOW = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/flow");
    public static final TagKey<BannerPattern> PATTERN_ITEM_GUSTER = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/guster");
    public static final TagKey<BannerPattern> PATTERN_ITEM_FIELD_MASONED = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/field_masoned");
    public static final TagKey<BannerPattern> PATTERN_ITEM_BORDURE_INDENTED = org.basaltmc.bukkit.facade.Tags.key(Registries.BANNER_PATTERN, "pattern_item/bordure_indented");

    private BannerPatternTags() {}
}

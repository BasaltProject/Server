// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.Map;
import net.minecraft.core.Direction;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.material.Fluids;

/** Picks the vanilla item class for a native item id. Generated from the vanilla registration order. */
public final class ItemClasses {
    private static final Map<String, String> kEntityTypes = Map.ofEntries(
        Map.entry("acacia_boat", "acacia_boat"),
        Map.entry("acacia_chest_boat", "acacia_chest_boat"),
        Map.entry("axolotl_bucket", "axolotl"),
        Map.entry("bamboo_chest_raft", "bamboo_chest_raft"),
        Map.entry("bamboo_raft", "bamboo_raft"),
        Map.entry("birch_boat", "birch_boat"),
        Map.entry("birch_chest_boat", "birch_chest_boat"),
        Map.entry("carrot_on_a_stick", "pig"),
        Map.entry("cherry_boat", "cherry_boat"),
        Map.entry("cherry_chest_boat", "cherry_chest_boat"),
        Map.entry("chest_minecart", "chest_minecart"),
        Map.entry("cod_bucket", "cod"),
        Map.entry("command_block_minecart", "command_block_minecart"),
        Map.entry("dark_oak_boat", "dark_oak_boat"),
        Map.entry("dark_oak_chest_boat", "dark_oak_chest_boat"),
        Map.entry("furnace_minecart", "furnace_minecart"),
        Map.entry("glow_item_frame", "glow_item_frame"),
        Map.entry("hopper_minecart", "hopper_minecart"),
        Map.entry("item_frame", "item_frame"),
        Map.entry("jungle_boat", "jungle_boat"),
        Map.entry("jungle_chest_boat", "jungle_chest_boat"),
        Map.entry("mangrove_boat", "mangrove_boat"),
        Map.entry("mangrove_chest_boat", "mangrove_chest_boat"),
        Map.entry("minecart", "minecart"),
        Map.entry("oak_boat", "oak_boat"),
        Map.entry("oak_chest_boat", "oak_chest_boat"),
        Map.entry("painting", "painting"),
        Map.entry("pale_oak_boat", "pale_oak_boat"),
        Map.entry("pale_oak_chest_boat", "pale_oak_chest_boat"),
        Map.entry("poplar_boat", "poplar_boat"),
        Map.entry("poplar_chest_boat", "poplar_chest_boat"),
        Map.entry("pufferfish_bucket", "pufferfish"),
        Map.entry("salmon_bucket", "salmon"),
        Map.entry("spruce_boat", "spruce_boat"),
        Map.entry("spruce_chest_boat", "spruce_chest_boat"),
        Map.entry("sulfur_cube_bucket", "sulfur_cube"),
        Map.entry("tadpole_bucket", "tadpole"),
        Map.entry("tnt_minecart", "tnt_minecart"),
        Map.entry("tropical_fish_bucket", "tropical_fish"),
        Map.entry("warped_fungus_on_a_stick", "strider")
    );

    private ItemClasses() {}

    static String entityTypeId(Item item) {
        return kEntityTypes.get(item.itemKey().getPath());
    }

    static net.minecraft.world.entity.EntityType<?> entityType(Item item) {
        return BuiltInRegistries.ENTITY_TYPE.getValue(Identifier.withDefaultNamespace(entityTypeId(item)));
    }

    private static Block block(String path) {
        return BuiltInRegistries.BLOCK.getValue(Identifier.withDefaultNamespace(path));
    }

    public static Item create(Identifier key) {
        Item.Properties p = new Item.Properties().setId(ResourceKey.create(net.minecraft.core.registries.Registries.ITEM, key));
        if (!key.getNamespace().equals("minecraft")) {
            return new Item(p);
        }
        String path = key.getPath();
        Item chosen = special(path, key, p);
        if (chosen != null) {
            return chosen;
        }
        Block plain = BuiltInRegistries.BLOCK.getOptional(Identifier.withDefaultNamespace(path)).orElse(null);
        return plain != null && !path.equals("air") ? new BlockItem(plain, p) : new Item(p);
    }

    private static Item special(String path, Identifier key, Item.Properties p) {
        return switch (path) {
            case "abandoned_camp_map", "buried_ancient_city_map", "buried_mineshaft_map", "buried_treasure_map", "buried_trial_chambers_map", "desert_pyramid_map", "desert_village_map", "filled_map", "jungle_pyramid_map", "ocean_monument_map", "plains_village_map", "savanna_village_map", "snowy_village_map", "swamp_hut_map", "taiga_village_map", "warm_ocean_ruins_map", "woodland_mansion_map" -> new MapItem(p);
            case "acacia_boat", "acacia_chest_boat", "bamboo_chest_raft", "bamboo_raft", "birch_boat", "birch_chest_boat", "cherry_boat", "cherry_chest_boat", "dark_oak_boat", "dark_oak_chest_boat", "jungle_boat", "jungle_chest_boat", "mangrove_boat", "mangrove_chest_boat", "oak_boat", "oak_chest_boat", "pale_oak_boat", "pale_oak_chest_boat", "poplar_boat", "poplar_chest_boat", "spruce_boat", "spruce_chest_boat" -> new BoatItem(null, p);
            case "acacia_door" -> new DoubleHighBlockItem(block("acacia_door"), p);
            case "acacia_hanging_sign" -> new HangingSignItem(block("acacia_hanging_sign"), block("acacia_wall_hanging_sign"), p);
            case "acacia_sign" -> new StandingAndWallBlockItem(block("acacia_sign"), block("acacia_wall_sign"), Direction.DOWN, p);
            case "air" -> new AirItem(p);
            case "armor_stand" -> new ArmorStandItem(p);
            case "arrow" -> new ArrowItem(p);
            case "axolotl_bucket" -> new MobBucketItem(null, Fluids.WATER, SoundEvents.BUCKET_EMPTY_AXOLOTL, p);
            case "bamboo_door" -> new DoubleHighBlockItem(block("bamboo_door"), p);
            case "bamboo_hanging_sign" -> new HangingSignItem(block("bamboo_hanging_sign"), block("bamboo_wall_hanging_sign"), p);
            case "bamboo_sign" -> new StandingAndWallBlockItem(block("bamboo_sign"), block("bamboo_wall_sign"), Direction.DOWN, p);
            case "beetroot_seeds" -> new BlockItem(block("beetroots"), p);
            case "birch_door" -> new DoubleHighBlockItem(block("birch_door"), p);
            case "birch_hanging_sign" -> new HangingSignItem(block("birch_hanging_sign"), block("birch_wall_hanging_sign"), p);
            case "birch_sign" -> new StandingAndWallBlockItem(block("birch_sign"), block("birch_wall_sign"), Direction.DOWN, p);
            case "black_banner" -> new BannerItem(block("black_banner"), block("black_wall_banner"), p);
            case "black_bundle", "blue_bundle", "brown_bundle", "bundle", "cyan_bundle", "gray_bundle", "green_bundle", "light_blue_bundle", "light_gray_bundle", "lime_bundle", "magenta_bundle", "orange_bundle", "pink_bundle", "purple_bundle", "red_bundle", "white_bundle", "yellow_bundle" -> new BundleItem(p);
            case "black_cushion", "blue_cushion", "brown_cushion", "cyan_cushion", "gray_cushion", "green_cushion", "light_blue_cushion", "light_gray_cushion", "lime_cushion", "magenta_cushion", "orange_cushion", "pink_cushion", "purple_cushion", "red_cushion", "white_cushion", "yellow_cushion" -> new CushionItem(p);
            case "black_dye", "blue_dye", "brown_dye", "cyan_dye", "gray_dye", "green_dye", "light_blue_dye", "light_gray_dye", "lime_dye", "magenta_dye", "orange_dye", "pink_dye", "purple_dye", "red_dye", "white_dye", "yellow_dye" -> new DyeItem(p);
            case "blue_banner" -> new BannerItem(block("blue_banner"), block("blue_wall_banner"), p);
            case "blue_egg", "brown_egg", "egg" -> new EggItem(p);
            case "bolt_armor_trim_smithing_template", "coast_armor_trim_smithing_template", "dune_armor_trim_smithing_template", "eye_armor_trim_smithing_template", "flow_armor_trim_smithing_template", "host_armor_trim_smithing_template", "raiser_armor_trim_smithing_template", "rib_armor_trim_smithing_template", "sentry_armor_trim_smithing_template", "shaper_armor_trim_smithing_template", "silence_armor_trim_smithing_template", "snout_armor_trim_smithing_template", "spire_armor_trim_smithing_template", "tide_armor_trim_smithing_template", "vex_armor_trim_smithing_template", "ward_armor_trim_smithing_template", "wayfinder_armor_trim_smithing_template", "wild_armor_trim_smithing_template" -> SmithingTemplateItem.createArmorTrimTemplate(p);
            case "bone_meal" -> new BoneMealItem(p);
            case "bow" -> new BowItem(p);
            case "brain_coral_fan" -> new StandingAndWallBlockItem(block("brain_coral_fan"), block("brain_coral_wall_fan"), Direction.DOWN, p);
            case "brown_banner" -> new BannerItem(block("brown_banner"), block("brown_wall_banner"), p);
            case "brush" -> new BrushItem(p);
            case "bubble_coral_fan" -> new StandingAndWallBlockItem(block("bubble_coral_fan"), block("bubble_coral_wall_fan"), Direction.DOWN, p);
            case "bucket" -> new BucketItem(Fluids.EMPTY, p);
            case "carrot" -> new BlockItem(block("carrots"), p);
            case "carrot_on_a_stick" -> new FoodOnAStickItem<>(null, 7, p);
            case "chain_command_block" -> new GameMasterBlockItem(block("chain_command_block"), p);
            case "cherry_door" -> new DoubleHighBlockItem(block("cherry_door"), p);
            case "cherry_hanging_sign" -> new HangingSignItem(block("cherry_hanging_sign"), block("cherry_wall_hanging_sign"), p);
            case "cherry_sign" -> new StandingAndWallBlockItem(block("cherry_sign"), block("cherry_wall_sign"), Direction.DOWN, p);
            case "chest_minecart", "command_block_minecart", "furnace_minecart", "hopper_minecart", "minecart", "tnt_minecart" -> new MinecartItem(null, p);
            case "cocoa_beans" -> new BlockItem(block("cocoa"), p);
            case "cod_bucket", "pufferfish_bucket", "salmon_bucket", "tropical_fish_bucket" -> new MobBucketItem(null, Fluids.WATER, SoundEvents.BUCKET_EMPTY_FISH, p);
            case "command_block" -> new GameMasterBlockItem(block("command_block"), p);
            case "compass" -> new CompassItem(p);
            case "copper_door" -> new DoubleHighBlockItem(block("copper_door"), p);
            case "copper_torch" -> new StandingAndWallBlockItem(block("copper_torch"), block("copper_wall_torch"), Direction.DOWN, p);
            case "creeper_head" -> new StandingAndWallBlockItem(block("creeper_head"), block("creeper_wall_head"), Direction.DOWN, p);
            case "crimson_door" -> new DoubleHighBlockItem(block("crimson_door"), p);
            case "crimson_hanging_sign" -> new HangingSignItem(block("crimson_hanging_sign"), block("crimson_wall_hanging_sign"), p);
            case "crimson_sign" -> new StandingAndWallBlockItem(block("crimson_sign"), block("crimson_wall_sign"), Direction.DOWN, p);
            case "crossbow" -> new CrossbowItem(p);
            case "cyan_banner" -> new BannerItem(block("cyan_banner"), block("cyan_wall_banner"), p);
            case "dark_oak_door" -> new DoubleHighBlockItem(block("dark_oak_door"), p);
            case "dark_oak_hanging_sign" -> new HangingSignItem(block("dark_oak_hanging_sign"), block("dark_oak_wall_hanging_sign"), p);
            case "dark_oak_sign" -> new StandingAndWallBlockItem(block("dark_oak_sign"), block("dark_oak_wall_sign"), Direction.DOWN, p);
            case "dead_brain_coral_fan" -> new StandingAndWallBlockItem(block("dead_brain_coral_fan"), block("dead_brain_coral_wall_fan"), Direction.DOWN, p);
            case "dead_bubble_coral_fan" -> new StandingAndWallBlockItem(block("dead_bubble_coral_fan"), block("dead_bubble_coral_wall_fan"), Direction.DOWN, p);
            case "dead_fire_coral_fan" -> new StandingAndWallBlockItem(block("dead_fire_coral_fan"), block("dead_fire_coral_wall_fan"), Direction.DOWN, p);
            case "dead_horn_coral_fan" -> new StandingAndWallBlockItem(block("dead_horn_coral_fan"), block("dead_horn_coral_wall_fan"), Direction.DOWN, p);
            case "dead_tube_coral_fan" -> new StandingAndWallBlockItem(block("dead_tube_coral_fan"), block("dead_tube_coral_wall_fan"), Direction.DOWN, p);
            case "debug_stick" -> new DebugStickItem(p);
            case "disc_fragment_5" -> new DiscFragmentItem(p);
            case "dragon_head" -> new StandingAndWallBlockItem(block("dragon_head"), block("dragon_wall_head"), Direction.DOWN, p);
            case "end_crystal" -> new EndCrystalItem(p);
            case "ender_eye" -> new EnderEyeItem(p);
            case "ender_pearl" -> new EnderpearlItem(p);
            case "experience_bottle" -> new ExperienceBottleItem(p);
            case "exposed_copper_door" -> new DoubleHighBlockItem(block("exposed_copper_door"), p);
            case "fire_charge" -> new FireChargeItem(p);
            case "fire_coral_fan" -> new StandingAndWallBlockItem(block("fire_coral_fan"), block("fire_coral_wall_fan"), Direction.DOWN, p);
            case "firework_rocket" -> new FireworkRocketItem(p);
            case "fishing_rod" -> new FishingRodItem(p);
            case "flint_and_steel" -> new FlintAndSteelItem(p);
            case "frogspawn" -> new PlaceOnWaterBlockItem(block("frogspawn"), p);
            case "glass_bottle" -> new BottleItem(p);
            case "glow_berries" -> new BlockItem(block("cave_vines"), p);
            case "glow_ink_sac" -> new GlowInkSacItem(p);
            case "glow_item_frame", "item_frame" -> new ItemFrameItem(null, p);
            case "goat_horn" -> new InstrumentItem(p);
            case "gray_banner" -> new BannerItem(block("gray_banner"), block("gray_wall_banner"), p);
            case "green_banner" -> new BannerItem(block("green_banner"), block("green_wall_banner"), p);
            case "honeycomb" -> new HoneycombItem(p);
            case "horn_coral_fan" -> new StandingAndWallBlockItem(block("horn_coral_fan"), block("horn_coral_wall_fan"), Direction.DOWN, p);
            case "ink_sac" -> new InkSacItem(p);
            case "iron_door" -> new DoubleHighBlockItem(block("iron_door"), p);
            case "jigsaw" -> new GameMasterBlockItem(block("jigsaw"), p);
            case "jungle_door" -> new DoubleHighBlockItem(block("jungle_door"), p);
            case "jungle_hanging_sign" -> new HangingSignItem(block("jungle_hanging_sign"), block("jungle_wall_hanging_sign"), p);
            case "jungle_sign" -> new StandingAndWallBlockItem(block("jungle_sign"), block("jungle_wall_sign"), Direction.DOWN, p);
            case "knowledge_book" -> new KnowledgeBookItem(p);
            case "large_fern" -> new DoubleHighBlockItem(block("large_fern"), p);
            case "lava_bucket" -> new BucketItem(Fluids.LAVA, p);
            case "lead" -> new LeadItem(p);
            case "light_blue_banner" -> new BannerItem(block("light_blue_banner"), block("light_blue_wall_banner"), p);
            case "light_gray_banner" -> new BannerItem(block("light_gray_banner"), block("light_gray_wall_banner"), p);
            case "lilac" -> new DoubleHighBlockItem(block("lilac"), p);
            case "lily_pad" -> new PlaceOnWaterBlockItem(block("lily_pad"), p);
            case "lime_banner" -> new BannerItem(block("lime_banner"), block("lime_wall_banner"), p);
            case "lingering_potion" -> new LingeringPotionItem(p);
            case "mace" -> new MaceItem(p);
            case "magenta_banner" -> new BannerItem(block("magenta_banner"), block("magenta_wall_banner"), p);
            case "mangrove_door" -> new DoubleHighBlockItem(block("mangrove_door"), p);
            case "mangrove_hanging_sign" -> new HangingSignItem(block("mangrove_hanging_sign"), block("mangrove_wall_hanging_sign"), p);
            case "mangrove_sign" -> new StandingAndWallBlockItem(block("mangrove_sign"), block("mangrove_wall_sign"), Direction.DOWN, p);
            case "map" -> new EmptyMapItem(p);
            case "melon_seeds" -> new BlockItem(block("melon_stem"), p);
            case "name_tag" -> new NameTagItem(p);
            case "netherite_upgrade_smithing_template" -> SmithingTemplateItem.createNetheriteUpgradeTemplate(p);
            case "oak_door" -> new DoubleHighBlockItem(block("oak_door"), p);
            case "oak_hanging_sign" -> new HangingSignItem(block("oak_hanging_sign"), block("oak_wall_hanging_sign"), p);
            case "oak_sign" -> new StandingAndWallBlockItem(block("oak_sign"), block("oak_wall_sign"), Direction.DOWN, p);
            case "orange_banner" -> new BannerItem(block("orange_banner"), block("orange_wall_banner"), p);
            case "oxidized_copper_door" -> new DoubleHighBlockItem(block("oxidized_copper_door"), p);
            case "painting" -> new HangingEntityItem(null, p);
            case "pale_oak_door" -> new DoubleHighBlockItem(block("pale_oak_door"), p);
            case "pale_oak_hanging_sign" -> new HangingSignItem(block("pale_oak_hanging_sign"), block("pale_oak_wall_hanging_sign"), p);
            case "pale_oak_sign" -> new StandingAndWallBlockItem(block("pale_oak_sign"), block("pale_oak_wall_sign"), Direction.DOWN, p);
            case "peony" -> new DoubleHighBlockItem(block("peony"), p);
            case "piglin_head" -> new StandingAndWallBlockItem(block("piglin_head"), block("piglin_wall_head"), Direction.DOWN, p);
            case "pink_banner" -> new BannerItem(block("pink_banner"), block("pink_wall_banner"), p);
            case "pitcher_pod" -> new BlockItem(block("pitcher_crop"), p);
            case "player_head" -> new PlayerHeadItem(block("player_head"), block("player_wall_head"), p);
            case "poplar_door" -> new DoubleHighBlockItem(block("poplar_door"), p);
            case "poplar_hanging_sign" -> new HangingSignItem(block("poplar_hanging_sign"), block("poplar_wall_hanging_sign"), p);
            case "poplar_sign" -> new StandingAndWallBlockItem(block("poplar_sign"), block("poplar_wall_sign"), Direction.DOWN, p);
            case "potato" -> new BlockItem(block("potatoes"), p);
            case "potion" -> new PotionItem(p);
            case "powder_snow_bucket" -> new SolidBucketItem(block("powder_snow"), SoundEvents.BUCKET_EMPTY_POWDER_SNOW, p);
            case "pumpkin_seeds" -> new BlockItem(block("pumpkin_stem"), p);
            case "purple_banner" -> new BannerItem(block("purple_banner"), block("purple_wall_banner"), p);
            case "red_banner" -> new BannerItem(block("red_banner"), block("red_wall_banner"), p);
            case "redstone" -> new BlockItem(block("redstone_wire"), p);
            case "redstone_torch" -> new StandingAndWallBlockItem(block("redstone_torch"), block("redstone_wall_torch"), Direction.DOWN, p);
            case "repeating_command_block" -> new GameMasterBlockItem(block("repeating_command_block"), p);
            case "rose_bush" -> new DoubleHighBlockItem(block("rose_bush"), p);
            case "scaffolding" -> new ScaffoldingBlockItem(block("scaffolding"), p);
            case "shears" -> new ShearsItem(p);
            case "shield" -> new ShieldItem(p);
            case "skeleton_skull" -> new StandingAndWallBlockItem(block("skeleton_skull"), block("skeleton_wall_skull"), Direction.DOWN, p);
            case "small_dripleaf" -> new DoubleHighBlockItem(block("small_dripleaf"), p);
            case "snowball" -> new SnowballItem(p);
            case "soul_torch" -> new StandingAndWallBlockItem(block("soul_torch"), block("soul_wall_torch"), Direction.DOWN, p);
            case "spectral_arrow" -> new SpectralArrowItem(p);
            case "splash_potion" -> new SplashPotionItem(p);
            case "spruce_door" -> new DoubleHighBlockItem(block("spruce_door"), p);
            case "spruce_hanging_sign" -> new HangingSignItem(block("spruce_hanging_sign"), block("spruce_wall_hanging_sign"), p);
            case "spruce_sign" -> new StandingAndWallBlockItem(block("spruce_sign"), block("spruce_wall_sign"), Direction.DOWN, p);
            case "spyglass" -> new SpyglassItem(p);
            case "string" -> new BlockItem(block("tripwire"), p);
            case "structure_block" -> new GameMasterBlockItem(block("structure_block"), p);
            case "sulfur_cube_bucket" -> new MobBucketItem(null, Fluids.EMPTY, SoundEvents.BUCKET_EMPTY_SULFUR_CUBE, p);
            case "sunflower" -> new DoubleHighBlockItem(block("sunflower"), p);
            case "sweet_berries" -> new BlockItem(block("sweet_berry_bush"), p);
            case "tadpole_bucket" -> new MobBucketItem(null, Fluids.WATER, SoundEvents.BUCKET_EMPTY_TADPOLE, p);
            case "tall_grass" -> new DoubleHighBlockItem(block("tall_grass"), p);
            case "test_block" -> new GameMasterBlockItem(block("test_block"), p);
            case "test_instance_block" -> new GameMasterBlockItem(block("test_instance_block"), p);
            case "tipped_arrow" -> new TippedArrowItem(p);
            case "torch" -> new StandingAndWallBlockItem(block("torch"), block("wall_torch"), Direction.DOWN, p);
            case "torchflower_seeds" -> new BlockItem(block("torchflower_crop"), p);
            case "trident" -> new TridentItem(p);
            case "tube_coral_fan" -> new StandingAndWallBlockItem(block("tube_coral_fan"), block("tube_coral_wall_fan"), Direction.DOWN, p);
            case "warped_door" -> new DoubleHighBlockItem(block("warped_door"), p);
            case "warped_fungus_on_a_stick" -> new FoodOnAStickItem<>(null, 1, p);
            case "warped_hanging_sign" -> new HangingSignItem(block("warped_hanging_sign"), block("warped_wall_hanging_sign"), p);
            case "warped_sign" -> new StandingAndWallBlockItem(block("warped_sign"), block("warped_wall_sign"), Direction.DOWN, p);
            case "water_bucket" -> new BucketItem(Fluids.WATER, p);
            case "waxed_copper_door" -> new DoubleHighBlockItem(block("waxed_copper_door"), p);
            case "waxed_exposed_copper_door" -> new DoubleHighBlockItem(block("waxed_exposed_copper_door"), p);
            case "waxed_oxidized_copper_door" -> new DoubleHighBlockItem(block("waxed_oxidized_copper_door"), p);
            case "waxed_weathered_copper_door" -> new DoubleHighBlockItem(block("waxed_weathered_copper_door"), p);
            case "weathered_copper_door" -> new DoubleHighBlockItem(block("weathered_copper_door"), p);
            case "wheat_seeds" -> new BlockItem(block("wheat"), p);
            case "white_banner" -> new BannerItem(block("white_banner"), block("white_wall_banner"), p);
            case "wind_charge" -> new WindChargeItem(p);
            case "wither_skeleton_skull" -> new StandingAndWallBlockItem(block("wither_skeleton_skull"), block("wither_skeleton_wall_skull"), Direction.DOWN, p);
            case "writable_book" -> new WritableBookItem(p);
            case "written_book" -> new WrittenBookItem(p);
            case "yellow_banner" -> new BannerItem(block("yellow_banner"), block("yellow_wall_banner"), p);
            case "zombie_head" -> new StandingAndWallBlockItem(block("zombie_head"), block("zombie_wall_head"), Direction.DOWN, p);
            default -> null;
        };
    }
}

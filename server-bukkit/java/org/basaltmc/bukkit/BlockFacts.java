package org.basaltmc.bukkit;

import java.util.Set;

final class BlockFacts {
    private static final Set<String> kInteractable = Set.of((
        "dispenser note_block straw_bed tnt chiseled_bookshelf acacia_shelf bamboo_shelf birch_shelf " +
        "cherry_shelf crimson_shelf dark_oak_shelf jungle_shelf mangrove_shelf oak_shelf pale_oak_shelf " +
        "poplar_shelf spruce_shelf warped_shelf chest redstone_wire crafting_table furnace oak_sign " +
        "spruce_sign birch_sign acacia_sign cherry_sign jungle_sign dark_oak_sign pale_oak_sign " +
        "mangrove_sign poplar_sign bamboo_sign oak_door oak_wall_sign spruce_wall_sign birch_wall_sign " +
        "acacia_wall_sign cherry_wall_sign jungle_wall_sign dark_oak_wall_sign pale_oak_wall_sign " +
        "mangrove_wall_sign poplar_wall_sign bamboo_wall_sign oak_hanging_sign spruce_hanging_sign " +
        "birch_hanging_sign acacia_hanging_sign cherry_hanging_sign jungle_hanging_sign " +
        "dark_oak_hanging_sign pale_oak_hanging_sign crimson_hanging_sign warped_hanging_sign " +
        "mangrove_hanging_sign poplar_hanging_sign bamboo_hanging_sign oak_wall_hanging_sign " +
        "spruce_wall_hanging_sign birch_wall_hanging_sign acacia_wall_hanging_sign cherry_wall_hanging_sign " +
        "jungle_wall_hanging_sign dark_oak_wall_hanging_sign pale_oak_wall_hanging_sign " +
        "mangrove_wall_hanging_sign poplar_wall_hanging_sign crimson_wall_hanging_sign " +
        "warped_wall_hanging_sign bamboo_wall_hanging_sign lever iron_door redstone_ore " +
        "deepslate_redstone_ore stone_button jukebox oak_fence cake repeater oak_trapdoor spruce_trapdoor " +
        "birch_trapdoor jungle_trapdoor acacia_trapdoor cherry_trapdoor dark_oak_trapdoor pale_oak_trapdoor " +
        "mangrove_trapdoor poplar_trapdoor bamboo_trapdoor pumpkin oak_fence_gate nether_brick_fence " +
        "enchanting_table brewing_stand cauldron water_cauldron lava_cauldron powder_snow_cauldron " +
        "dragon_egg ender_chest command_block beacon flower_pot potted_torchflower potted_oak_sapling " +
        "potted_spruce_sapling potted_birch_sapling potted_jungle_sapling potted_acacia_sapling " +
        "potted_cherry_sapling potted_dark_oak_sapling potted_pale_oak_sapling potted_poplar_sapling " +
        "potted_mangrove_propagule potted_fern potted_dandelion potted_golden_dandelion potted_poppy " +
        "potted_blue_orchid potted_allium potted_azure_bluet potted_red_tulip potted_orange_tulip " +
        "potted_white_tulip potted_pink_tulip potted_oxeye_daisy potted_cornflower potted_lily_of_the_valley " +
        "potted_wither_rose potted_red_mushroom potted_brown_mushroom potted_dead_bush potted_cactus " +
        "oak_button spruce_button birch_button jungle_button acacia_button cherry_button dark_oak_button " +
        "pale_oak_button mangrove_button poplar_button bamboo_button anvil chipped_anvil damaged_anvil " +
        "trapped_chest comparator daylight_detector hopper dropper light iron_trapdoor spruce_fence_gate " +
        "birch_fence_gate jungle_fence_gate acacia_fence_gate cherry_fence_gate dark_oak_fence_gate " +
        "pale_oak_fence_gate mangrove_fence_gate poplar_fence_gate bamboo_fence_gate spruce_fence " +
        "birch_fence jungle_fence acacia_fence cherry_fence dark_oak_fence pale_oak_fence mangrove_fence " +
        "poplar_fence bamboo_fence spruce_door birch_door jungle_door acacia_door cherry_door dark_oak_door " +
        "pale_oak_door mangrove_door poplar_door bamboo_door repeating_command_block chain_command_block " +
        "shulker_box potted_bamboo loom barrel smoker blast_furnace cartography_table grindstone lectern " +
        "smithing_table stonecutter bell campfire soul_campfire sweet_berry_bush crimson_fence warped_fence " +
        "crimson_trapdoor warped_trapdoor crimson_fence_gate warped_fence_gate crimson_button warped_button " +
        "crimson_door warped_door crimson_sign warped_sign crimson_wall_sign warped_wall_sign " +
        "structure_block jigsaw test_block test_instance_block composter bee_nest beehive respawn_anchor " +
        "potted_crimson_fungus potted_warped_fungus potted_crimson_roots potted_warped_roots " +
        "polished_blackstone_button candle candle_cake cave_vines cave_vines_plant potted_azalea " +
        "potted_flowering_azalea decorated_pot crafter vault potted_open_eyeblossom potted_closed_eyeblossom ").trim().split(" "));
    private static final Set<String> kIgnitedByLava = Set.of((
        "oak_planks spruce_planks birch_planks jungle_planks acacia_planks cherry_planks dark_oak_planks " +
        "pale_oak_wood pale_oak_planks mangrove_planks poplar_planks bamboo_planks bamboo_mosaic " +
        "mangrove_roots oak_wood spruce_wood birch_wood jungle_wood acacia_wood cherry_wood dark_oak_wood " +
        "mangrove_wood poplar_wood stripped_oak_wood stripped_spruce_wood stripped_birch_wood " +
        "stripped_jungle_wood stripped_acacia_wood stripped_cherry_wood stripped_dark_oak_wood " +
        "stripped_pale_oak_wood stripped_poplar_wood cherry_leaves pale_oak_leaves note_block straw_bed " +
        "short_grass fern dead_bush bush red_shrub short_dry_grass tall_dry_grass tnt bookshelf " +
        "chiseled_bookshelf acacia_shelf bamboo_shelf birch_shelf cherry_shelf crimson_shelf dark_oak_shelf " +
        "jungle_shelf mangrove_shelf oak_shelf pale_oak_shelf poplar_shelf spruce_shelf warped_shelf chest " +
        "crafting_table oak_sign spruce_sign birch_sign acacia_sign cherry_sign jungle_sign dark_oak_sign " +
        "pale_oak_sign mangrove_sign poplar_sign bamboo_sign oak_door oak_wall_sign spruce_wall_sign " +
        "birch_wall_sign acacia_wall_sign cherry_wall_sign jungle_wall_sign dark_oak_wall_sign " +
        "pale_oak_wall_sign mangrove_wall_sign poplar_wall_sign bamboo_wall_sign oak_hanging_sign " +
        "spruce_hanging_sign birch_hanging_sign acacia_hanging_sign cherry_hanging_sign jungle_hanging_sign " +
        "dark_oak_hanging_sign pale_oak_hanging_sign mangrove_hanging_sign poplar_hanging_sign " +
        "bamboo_hanging_sign oak_wall_hanging_sign spruce_wall_hanging_sign birch_wall_hanging_sign " +
        "acacia_wall_hanging_sign cherry_wall_hanging_sign jungle_wall_hanging_sign " +
        "dark_oak_wall_hanging_sign pale_oak_wall_hanging_sign mangrove_wall_hanging_sign " +
        "poplar_wall_hanging_sign bamboo_wall_hanging_sign oak_pressure_plate spruce_pressure_plate " +
        "birch_pressure_plate jungle_pressure_plate acacia_pressure_plate cherry_pressure_plate " +
        "dark_oak_pressure_plate pale_oak_pressure_plate mangrove_pressure_plate poplar_pressure_plate " +
        "bamboo_pressure_plate cactus_flower jukebox oak_fence oak_trapdoor spruce_trapdoor birch_trapdoor " +
        "jungle_trapdoor acacia_trapdoor cherry_trapdoor dark_oak_trapdoor pale_oak_trapdoor " +
        "mangrove_trapdoor poplar_trapdoor bamboo_trapdoor brown_mushroom_block red_mushroom_block " +
        "mushroom_stem vine glow_lichen resin_clump oak_fence_gate trapped_chest daylight_detector sunflower " +
        "lilac rose_bush peony tall_grass large_fern spruce_fence_gate birch_fence_gate jungle_fence_gate " +
        "acacia_fence_gate cherry_fence_gate dark_oak_fence_gate pale_oak_fence_gate mangrove_fence_gate " +
        "poplar_fence_gate bamboo_fence_gate spruce_fence birch_fence jungle_fence acacia_fence cherry_fence " +
        "dark_oak_fence pale_oak_fence mangrove_fence poplar_fence bamboo_fence spruce_door birch_door " +
        "jungle_door acacia_door cherry_door dark_oak_door pale_oak_door mangrove_door poplar_door " +
        "bamboo_door pitcher_plant bamboo_sapling bamboo loom barrel cartography_table fletching_table " +
        "lectern smithing_table campfire soul_campfire composter bee_nest beehive hanging_roots " +
        "pale_moss_block pale_moss_carpet pale_hanging_moss firefly_bush ").trim().split(" "));

    private BlockFacts() {}

    static boolean interactable(String path) {
        return kInteractable.contains(path);
    }

    static boolean ignitedByLava(String path) {
        return kIgnitedByLava.contains(path);
    }
}

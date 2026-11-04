// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import java.util.Set;

/** Picks the vanilla block class for a native block id, falling back to the plain table-backed block. */
final class BlockClasses {
    private static final Set<String> kFlowers = Set.of("dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "red_tulip",
        "orange_tulip", "white_tulip", "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley");
    private static final Set<String> kTallPlants = Set.of("tall_grass", "large_fern", "lilac", "rose_bush", "peony", "sunflower");

    private BlockClasses() {}

    static Block createEntity(int index, String id) {
        String path = id.startsWith("minecraft:") ? id.substring(10) : id;
        if (path.endsWith("_shulker_box") || path.equals("shulker_box")) {
            return new ShulkerBoxBlock(index);
        }
        if (path.endsWith("_wall_hanging_sign")) {
            return new WallHangingSignBlock(index);
        }
        if (path.endsWith("_hanging_sign")) {
            return new CeilingHangingSignBlock(index);
        }
        if (path.endsWith("_wall_sign")) {
            return new WallSignBlock(index);
        }
        if (path.endsWith("_sign")) {
            return new StandingSignBlock(index);
        }
        if (path.endsWith("_wall_banner")) {
            return new WallBannerBlock(index);
        }
        if (path.endsWith("_banner")) {
            return new BannerBlock(index);
        }
        if (path.endsWith("_wall_skull") || path.endsWith("_wall_head")) {
            return new WallSkullBlock(index);
        }
        if (path.endsWith("_skull") || path.endsWith("_head")) {
            return new SkullBlock(index);
        }
        switch (path) {
            case "chest": return new ChestBlock(index);
            case "trapped_chest": return new TrappedChestBlock(index);
            case "ender_chest": return new EnderChestBlock(index);
            case "barrel": return new BarrelBlock(index);
            case "dispenser": return new DispenserBlock(index);
            case "dropper": return new DropperBlock(index);
            case "hopper": return new HopperBlock(index);
            case "furnace": return new FurnaceBlock(index);
            case "blast_furnace": return new BlastFurnaceBlock(index);
            case "smoker": return new SmokerBlock(index);
            case "brewing_stand": return new BrewingStandBlock(index);
            case "beacon": return new BeaconBlock(index);
            case "enchanting_table": return new EnchantingTableBlock(index);
            case "lectern": return new LecternBlock(index);
            case "jukebox": return new JukeboxBlock(index);
            case "bell": return new BellBlock(index);
            case "campfire", "soul_campfire": return new CampfireBlock(index);
            case "spawner": return new SpawnerBlock(index);
            case "command_block", "chain_command_block", "repeating_command_block": return new CommandBlock(index);
            case "structure_block": return new StructureBlock(index);
            case "jigsaw": return new JigsawBlock(index);
            case "conduit": return new ConduitBlock(index);
            case "daylight_detector": return new DaylightDetectorBlock(index);
            case "decorated_pot": return new DecoratedPotBlock(index);
            case "crafter": return new CrafterBlock(index);
            case "sculk_sensor": return new SculkSensorBlock(index);
            case "sculk_shrieker": return new SculkShriekerBlock(index);
            case "sculk_catalyst": return new SculkCatalystBlock(index);
            case "beehive", "bee_nest": return new BeehiveBlock(index);
            case "creaking_heart": return new CreakingHeartBlock(index);
            case "trial_spawner": return new TrialSpawnerBlock(index);
            case "vault": return new VaultBlock(index);
            default: return null;
        }
    }

    static Block create(int index, String id) {
        String path = id.startsWith("minecraft:") ? id.substring(10) : id;
        if (path.endsWith("_bed")) {
            return new BedBlock(index);
        }
        if (path.endsWith("_slab")) {
            return new SlabBlock(index);
        }
        if (path.endsWith("_stairs")) {
            return new StairBlock(index);
        }
        if (path.endsWith("_trapdoor")) {
            return new TrapDoorBlock(index);
        }
        if (path.endsWith("_door")) {
            return new DoorBlock(index);
        }
        if (path.endsWith("_fence_gate")) {
            return new FenceGateBlock(index);
        }
        if (path.endsWith("_fence")) {
            return new FenceBlock(index);
        }
        if (path.endsWith("_wall")) {
            return new WallBlock(index);
        }
        if (path.endsWith("_leaves")) {
            return new LeavesBlock(index);
        }
        if (path.endsWith("_button")) {
            return new ButtonBlock(index);
        }
        if (path.equals("light_weighted_pressure_plate") || path.equals("heavy_weighted_pressure_plate")) {
            return new WeightedPressurePlateBlock(index);
        }
        if (path.endsWith("_pressure_plate")) {
            return new PressurePlateBlock(index);
        }
        if (path.endsWith("_sapling")) {
            return new SaplingBlock(index);
        }
        if (path.endsWith("_log") || path.endsWith("_wood") || path.equals("hay_block") || path.equals("bone_block")) {
            return new RotatedPillarBlock(index);
        }
        if (kFlowers.contains(path)) {
            return new FlowerBlock(index);
        }
        if (kTallPlants.contains(path)) {
            return new DoublePlantBlock(index);
        }
        switch (path) {
            case "wheat", "carrots", "potatoes":
                return new CropBlock(index);
            case "piston", "sticky_piston":
                return new net.minecraft.world.level.block.piston.PistonBaseBlock(index);
            case "piston_head":
                return new net.minecraft.world.level.block.piston.PistonHeadBlock(index);
            case "redstone_wire":
                return new RedstoneWireBlock(index);
            case "redstone_lamp":
                return new RedstoneLampBlock(index);
            case "water", "lava":
                return new LiquidBlock(index);
            case "comparator":
                return new ComparatorBlock(index);
            case "vine":
                return new VineBlock(index);
            case "repeater":
                return new RepeaterBlock(index);
            case "observer":
                return new ObserverBlock(index);
            case "torch", "soul_torch":
                return new TorchBlock(index);
            case "redstone_torch":
                return new RedstoneTorchBlock(index);
            case "lever":
                return new LeverBlock(index);
            case "dead_bush":
                return new BushBlock(index);
            case "ladder":
                return new LadderBlock(index);
            case "snow":
                return new SnowLayerBlock(index);
            case "grass_block":
                return new GrassBlock(index);
            case "cactus":
                return new CactusBlock(index);
            case "sugar_cane":
                return new SugarCaneBlock(index);
            case "anvil", "chipped_anvil", "damaged_anvil":
                return new AnvilBlock(index);
            case "ice":
                return new IceBlock(index);
            case "tnt":
                return new TntBlock(index);
            case "farmland":
                return new FarmlandBlock(index);
            case "fire":
                return new FireBlock(index);
            case "sand", "red_sand":
                return new SandBlock(index);
            default:
                return new Block(index);
        }
    }
}

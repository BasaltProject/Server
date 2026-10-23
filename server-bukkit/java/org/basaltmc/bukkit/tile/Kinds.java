package org.basaltmc.bukkit.tile;

import io.papermc.paper.block.LockableTileState;
import java.util.Map;
import org.bukkit.block.BlockState;
import org.bukkit.block.BrushableBlock;
import org.bukkit.block.CalibratedSculkSensor;
import org.bukkit.block.Comparator;
import org.bukkit.block.CopperGolemStatue;
import org.bukkit.block.DaylightDetector;
import org.bukkit.block.EnchantingTable;
import org.bukkit.block.HangingSign;
import org.bukkit.block.Jigsaw;
import org.bukkit.block.PotentSulfur;
import org.bukkit.block.SculkCatalyst;
import org.bukkit.block.SculkSensor;
import org.bukkit.block.SculkShrieker;
import org.bukkit.block.Sign;
import org.bukkit.block.SuspiciousSand;
import org.bukkit.block.TestBlock;
import org.bukkit.block.TestInstanceBlock;

/** Which kind of tile state each block id has. */
final class Kinds {
    private Kinds() {}

    private static final class Plain implements TileKind {
        private final Class<? extends BlockState> face;

        Plain(Class<? extends BlockState> face) {
            this.face = face;
        }

        @Override
        public Class<? extends BlockState> face() {
            return face;
        }

        @Override
        public Object call(Tile tile, java.lang.reflect.Method method, String name, Object[] args) {
            return Tile.kNone;
        }
    }

    private static final Map<String, TileKind> kNamed = Map.ofEntries(
        Map.entry("spawner", new SpawnerKind()),
        Map.entry("beacon", new WorldKinds.BeaconKind()),
        Map.entry("jukebox", new StorageKinds.JukeboxKind()),
        Map.entry("command_block", new WorldKinds.CommandBlockKind()),
        Map.entry("chain_command_block", new WorldKinds.CommandBlockKind()),
        Map.entry("repeating_command_block", new WorldKinds.CommandBlockKind()),
        Map.entry("structure_block", new WorldKinds.StructureKind()),
        Map.entry("lectern", new StorageKinds.LecternKind()),
        Map.entry("chiseled_bookshelf", new StorageKinds.ChiseledBookshelfKind()),
        Map.entry("decorated_pot", new StorageKinds.PotKind()),
        Map.entry("campfire", new WorldKinds.CampfireKind()),
        Map.entry("soul_campfire", new WorldKinds.CampfireKind()),
        Map.entry("bell", new WorldKinds.BellKind()),
        Map.entry("ender_chest", new WorldKinds.EnderChestKind()),
        Map.entry("end_gateway", new WorldKinds.GatewayKind()),
        Map.entry("sculk_sensor", new ActorKinds.SculkKind(SculkSensor.class, 8)),
        Map.entry("calibrated_sculk_sensor", new ActorKinds.SculkKind(CalibratedSculkSensor.class, 16)),
        Map.entry("sculk_shrieker", new ActorKinds.SculkKind(SculkShrieker.class, 0)),
        Map.entry("sculk_catalyst", new ActorKinds.SculkKind(SculkCatalyst.class, 0)),
        Map.entry("vault", new ActorKinds.VaultKind()),
        Map.entry("beehive", new ActorKinds.HiveKind()),
        Map.entry("bee_nest", new ActorKinds.HiveKind()),
        Map.entry("suspicious_sand", new ActorKinds.BrushableKind(SuspiciousSand.class)),
        Map.entry("suspicious_gravel", new ActorKinds.BrushableKind(BrushableBlock.class)),
        Map.entry("creaking_heart", new ActorKinds.CreakingKind()),
        Map.entry("trial_spawner", new ActorKinds.TrialKind()),
        Map.entry("enchanting_table", new Plain(EnchantingTable.class)),
        Map.entry("comparator", new Plain(Comparator.class)),
        Map.entry("daylight_detector", new Plain(DaylightDetector.class)),
        Map.entry("jigsaw", new Plain(Jigsaw.class)),
        Map.entry("test_block", new Plain(TestBlock.class)),
        Map.entry("test_instance_block", new Plain(TestInstanceBlock.class)),
        Map.entry("potent_sulfur", new Plain(PotentSulfur.class)));

    private static final TileKind kSign = new SignKind(Sign.class);
    private static final TileKind kHangingSign = new SignKind(HangingSign.class);
    private static final TileKind kSkull = new SkullKind();
    private static final TileKind kBanner = new BannerKind(false);
    private static final TileKind kBed = new BannerKind(true);
    private static final TileKind kShelf = new StorageKinds.ShelfKind();
    private static final TileKind kStatue = new Plain(CopperGolemStatue.class);

    /** The kind of a container block, whose own accessors the container state already holds. */
    static TileKind container(BlockState held) {
        for (Class<?> face : held.getClass().getInterfaces()) {
            if (BlockState.class.isAssignableFrom(face)) {
                @SuppressWarnings("unchecked")
                Class<? extends BlockState> state = (Class<? extends BlockState>) face;
                return new Plain(state);
            }
        }
        return null;
    }

    static TileKind of(String block) {
        TileKind named = kNamed.get(block);
        if (named != null) {
            return named;
        }
        if (block.endsWith("hanging_sign")) {
            return kHangingSign;
        }
        if (block.endsWith("_sign")) {
            return kSign;
        }
        if (block.endsWith("_head") || block.endsWith("_skull")) {
            return kSkull;
        }
        if (block.endsWith("_banner")) {
            return kBanner;
        }
        if (block.endsWith("_bed")) {
            return kBed;
        }
        if (block.endsWith("_shelf")) {
            return kShelf;
        }
        if (block.endsWith("copper_golem_statue")) {
            return kStatue;
        }
        return null;
    }
}

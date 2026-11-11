// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Registry;
import net.minecraft.util.Unit;
import net.minecraft.world.level.redstone.Orientation;
public class DebugSubscriptions<T> {
    public static final DebugSubscription<?> DEDICATED_SERVER_TICK_TIME = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugBeeInfo> BEES = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugBrainDump> BRAINS = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugBreezeInfo> BREEZES = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugGoalInfo> GOAL_SELECTORS = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugPathInfo> ENTITY_PATHS = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugEntityBlockIntersection> ENTITY_BLOCK_INTERSECTIONS = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugHiveInfo> BEE_HIVES = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugPoiInfo> POIS = new DebugSubscription<>(null);
    public static final DebugSubscription<Orientation> REDSTONE_WIRE_ORIENTATIONS = new DebugSubscription<>(null);
    public static final DebugSubscription<Unit> VILLAGE_SECTIONS = new DebugSubscription<>(null);
    public static final DebugSubscription<List<BlockPos>> RAIDS = new DebugSubscription<>(null);
    public static final DebugSubscription<List<DebugStructureInfo>> STRUCTURES = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugGameEventListenerInfo> GAME_EVENT_LISTENERS = new DebugSubscription<>(null);
    public static final DebugSubscription<BlockPos> NEIGHBOR_UPDATES = new DebugSubscription<>(null);
    public static final DebugSubscription<DebugGameEventInfo> GAME_EVENTS = new DebugSubscription<>(null);
    public static DebugSubscription<?> bootstrap(Registry<DebugSubscription<?>> registry) {
        return DEDICATED_SERVER_TICK_TIME;
    }
}

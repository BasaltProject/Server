// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.clock;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
public interface ClockTimeMarkers {
    ResourceKey<? extends Registry<ClockTimeMarker>> ROOT_ID = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("clock_time_marker"));
    ResourceKey<ClockTimeMarker> DAY = marker("day");
    ResourceKey<ClockTimeMarker> NOON = marker("noon");
    ResourceKey<ClockTimeMarker> NIGHT = marker("night");
    ResourceKey<ClockTimeMarker> MIDNIGHT = marker("midnight");
    ResourceKey<ClockTimeMarker> WAKE_UP_FROM_SLEEP = marker("wake_up_from_sleep");
    ResourceKey<ClockTimeMarker> ROLL_VILLAGE_SIEGE = marker("roll_village_siege");
    private static ResourceKey<ClockTimeMarker> marker(String name) {
        return createKey(name);
    }
    static ResourceKey<ClockTimeMarker> createKey(String name) {
        return ResourceKey.create(ROOT_ID, Identifier.withDefaultNamespace(name));
    }
}

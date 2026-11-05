// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.sniffer;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class SnifferAi {
    public static List<ActivityData<Sniffer>> getActivities() {
        return List.of(
            ActivityData.<Sniffer>nativeOnly(Activity.CORE),
            ActivityData.<Sniffer>nativeOnly(Activity.SNIFF),
            ActivityData.<Sniffer>nativeOnly(Activity.DIG),
            ActivityData.<Sniffer>nativeOnly(Activity.IDLE));
    }

    public static void updateActivity(Sniffer body) {
    }
}

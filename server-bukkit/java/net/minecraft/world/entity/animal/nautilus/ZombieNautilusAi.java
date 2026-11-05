// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.nautilus;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class ZombieNautilusAi {
    public static List<ActivityData<ZombieNautilus>> getActivities() {
        return List.of(
            ActivityData.<ZombieNautilus>nativeOnly(Activity.CORE),
            ActivityData.<ZombieNautilus>nativeOnly(Activity.IDLE),
            ActivityData.<ZombieNautilus>nativeOnly(Activity.FIGHT));
    }

    public static void updateActivity(ZombieNautilus body) {
    }
}

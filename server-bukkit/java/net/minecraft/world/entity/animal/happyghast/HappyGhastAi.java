// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.happyghast;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class HappyGhastAi {
    protected static List<ActivityData<HappyGhast>> getActivities() {
        return List.of(
            ActivityData.<HappyGhast>nativeOnly(Activity.CORE),
            ActivityData.<HappyGhast>nativeOnly(Activity.IDLE),
            ActivityData.<HappyGhast>nativeOnly(Activity.PANIC));
    }

    public static void updateActivity(HappyGhast body) {
    }
}

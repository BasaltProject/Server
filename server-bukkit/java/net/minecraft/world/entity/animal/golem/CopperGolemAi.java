// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.golem;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class CopperGolemAi {
    protected static List<ActivityData<CopperGolem>> getActivities() {
        return List.of(
            ActivityData.<CopperGolem>nativeOnly(Activity.CORE),
            ActivityData.<CopperGolem>nativeOnly(Activity.IDLE));
    }

    public static void updateActivity(CopperGolem body) {
    }
}

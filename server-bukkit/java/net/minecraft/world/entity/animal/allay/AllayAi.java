// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.allay;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class AllayAi {
    protected static List<ActivityData<Allay>> getActivities() {
        return List.of(
            ActivityData.<Allay>nativeOnly(Activity.CORE),
            ActivityData.<Allay>nativeOnly(Activity.IDLE));
    }

    public static void updateActivity(Allay body) {
    }
}

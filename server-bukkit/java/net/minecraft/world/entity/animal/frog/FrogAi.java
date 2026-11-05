// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.frog;

import java.util.List;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class FrogAi {
    protected static List<ActivityData<Frog>> getActivities() {
        return List.of(
            ActivityData.<Frog>nativeOnly(Activity.CORE),
            ActivityData.<Frog>nativeOnly(Activity.IDLE),
            ActivityData.<Frog>nativeOnly(Activity.SWIM),
            ActivityData.<Frog>nativeOnly(Activity.LAY_SPAWN),
            ActivityData.<Frog>nativeOnly(Activity.LONG_JUMP),
            ActivityData.<Frog>nativeOnly(Activity.TONGUE));
    }

    public static void updateActivity(Frog body) {
    }

    protected static void initMemories(Frog body, RandomSource random) {
    }
}

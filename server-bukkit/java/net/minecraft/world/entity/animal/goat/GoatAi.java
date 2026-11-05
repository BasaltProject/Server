// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.goat;

import java.util.List;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class GoatAi {
    protected static List<ActivityData<Goat>> getActivities() {
        return List.of(
            ActivityData.<Goat>nativeOnly(Activity.CORE),
            ActivityData.<Goat>nativeOnly(Activity.IDLE),
            ActivityData.<Goat>nativeOnly(Activity.LONG_JUMP),
            ActivityData.<Goat>nativeOnly(Activity.RAM));
    }

    public static void updateActivity(Goat body) {
    }

    protected static void initMemories(Goat body, RandomSource random) {
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class PiglinBruteAi {
    public static List<ActivityData<PiglinBrute>> getActivities(PiglinBrute piglin) {
        return List.of(
            ActivityData.<PiglinBrute>nativeOnly(Activity.CORE),
            ActivityData.<PiglinBrute>nativeOnly(Activity.IDLE),
            ActivityData.<PiglinBrute>nativeOnly(Activity.FIGHT));
    }

    protected static void updateActivity(PiglinBrute body) {
    }

    protected static void initMemories(PiglinBrute body) {
    }
}

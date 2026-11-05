// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.creaking;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class CreakingAi {
    protected static List<ActivityData<Creaking>> getActivities(Creaking creaking) {
        return List.of(
            ActivityData.<Creaking>nativeOnly(Activity.CORE),
            ActivityData.<Creaking>nativeOnly(Activity.IDLE),
            ActivityData.<Creaking>nativeOnly(Activity.FIGHT));
    }

    public static void updateActivity(Creaking creaking) {
    }
}

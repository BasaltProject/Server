// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.breeze;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class BreezeAi {
    protected static List<ActivityData<Breeze>> getActivities(Breeze breeze) {
        return List.of(
            ActivityData.<Breeze>nativeOnly(Activity.CORE),
            ActivityData.<Breeze>nativeOnly(Activity.IDLE),
            ActivityData.<Breeze>nativeOnly(Activity.FIGHT));
    }

    public static void updateActivity(Breeze body) {
    }
}

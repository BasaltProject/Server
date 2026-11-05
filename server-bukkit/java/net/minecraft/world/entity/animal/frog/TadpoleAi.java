// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.frog;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class TadpoleAi {
    public static List<ActivityData<Tadpole>> getActivities() {
        return List.of(
            ActivityData.<Tadpole>nativeOnly(Activity.CORE),
            ActivityData.<Tadpole>nativeOnly(Activity.IDLE));
    }

    public static void updateActivity(Tadpole body) {
    }
}

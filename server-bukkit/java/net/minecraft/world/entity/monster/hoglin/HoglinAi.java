// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.hoglin;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class HoglinAi {
    protected static List<ActivityData<Hoglin>> getActivities() {
        return List.of(
            ActivityData.<Hoglin>nativeOnly(Activity.CORE),
            ActivityData.<Hoglin>nativeOnly(Activity.IDLE),
            ActivityData.<Hoglin>nativeOnly(Activity.FIGHT),
            ActivityData.<Hoglin>nativeOnly(Activity.AVOID));
    }

    protected static void updateActivity(Hoglin body) {
    }
}

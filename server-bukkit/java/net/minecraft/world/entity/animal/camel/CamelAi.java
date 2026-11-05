// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.camel;

import java.util.List;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class CamelAi {
    protected static List<ActivityData<Camel>> getActivities() {
        return List.of(
            ActivityData.<Camel>nativeOnly(Activity.CORE),
            ActivityData.<Camel>nativeOnly(Activity.IDLE));
    }

    public static void updateActivity(Camel body) {
    }

    protected static void initMemories(Camel body, RandomSource random) {
    }
}

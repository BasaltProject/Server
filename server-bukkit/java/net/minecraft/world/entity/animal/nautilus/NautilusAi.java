// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.nautilus;

import java.util.List;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class NautilusAi {
    public static List<ActivityData<Nautilus>> getActivities() {
        return List.of(
            ActivityData.<Nautilus>nativeOnly(Activity.CORE),
            ActivityData.<Nautilus>nativeOnly(Activity.IDLE),
            ActivityData.<Nautilus>nativeOnly(Activity.FIGHT));
    }

    public static void updateActivity(Nautilus body) {
    }

    protected static void initMemories(AbstractNautilus body, RandomSource random) {
    }
}

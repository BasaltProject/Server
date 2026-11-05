// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.armadillo;

import java.util.List;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class ArmadilloAi {
    protected static List<ActivityData<Armadillo>> getActivities() {
        return List.of(
            ActivityData.<Armadillo>nativeOnly(Activity.CORE),
            ActivityData.<Armadillo>nativeOnly(Activity.IDLE),
            ActivityData.<Armadillo>nativeOnly(Activity.PANIC));
    }

    public static void updateActivity(Armadillo body) {
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.warden;

import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.schedule.Activity;
import org.basaltmc.bukkit.NativeBrain;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class WardenAi {
    public static final int EMERGE_DURATION = Mth.ceil(133.59999F);
    public static final int ROAR_DURATION = Mth.ceil(84.0F);
    public static final int DIGGING_COOLDOWN = 1200;

    public static void setDisturbanceLocation(Warden body, BlockPos position) {
        new NativeBrain(body).call("warden_disturb", (long) position.getX(), (long) position.getY(), (long) position.getZ());
    }

    protected static List<ActivityData<Warden>> getActivities(Warden body) {
        return List.of(
            ActivityData.<Warden>nativeOnly(Activity.CORE),
            ActivityData.<Warden>nativeOnly(Activity.EMERGE),
            ActivityData.<Warden>nativeOnly(Activity.DIG),
            ActivityData.<Warden>nativeOnly(Activity.IDLE),
            ActivityData.<Warden>nativeOnly(Activity.INVESTIGATE),
            ActivityData.<Warden>nativeOnly(Activity.SNIFF),
            ActivityData.<Warden>nativeOnly(Activity.ROAR),
            ActivityData.<Warden>nativeOnly(Activity.FIGHT));
    }

    public static void updateActivity(Brain<Warden> brain) {
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

import java.util.List;
import net.minecraft.tags.ItemTags;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.schedule.Activity;

/** Adapter: the native brain of this species owns the activity, the memories and every behavior. */
public class PiglinAi {
    public static boolean isWearingSafeArmor(LivingEntity wearer) {
        for (net.minecraft.world.entity.EquipmentSlot slot : net.minecraft.world.entity.EquipmentSlotGroup.ARMOR) {
            if (wearer.getItemBySlot(slot).is(ItemTags.PIGLIN_SAFE_ARMOR)) {
                return true;
            }
        }
        return false;
    }

    public static boolean isZombified(Entity entity) {
        return entity.getType() == EntityTypes.ZOMBIFIED_PIGLIN;
    }

    public static List<ActivityData<Piglin>> getActivities(Piglin piglin) {
        return List.of(
            ActivityData.<Piglin>nativeOnly(Activity.CORE),
            ActivityData.<Piglin>nativeOnly(Activity.IDLE),
            ActivityData.<Piglin>nativeOnly(Activity.FIGHT),
            ActivityData.<Piglin>nativeOnly(Activity.CELEBRATE),
            ActivityData.<Piglin>nativeOnly(Activity.ADMIRE_ITEM),
            ActivityData.<Piglin>nativeOnly(Activity.AVOID),
            ActivityData.<Piglin>nativeOnly(Activity.RIDE));
    }

    protected static void updateActivity(Piglin body) {
    }

    protected static void initMemories(Piglin body, RandomSource random) {
    }
}

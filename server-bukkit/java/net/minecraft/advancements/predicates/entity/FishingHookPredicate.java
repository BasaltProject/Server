// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record FishingHookPredicate(Optional<Boolean> inOpenWater) implements EntitySubPredicate {
    public static final FishingHookPredicate ANY = new FishingHookPredicate(Optional.empty());
    public static final Codec<FishingHookPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.BOOL.optionalFieldOf("in_open_water").forGetter(FishingHookPredicate::inOpenWater)).apply(i, FishingHookPredicate::new));
    public static FishingHookPredicate inOpenWater(boolean requirement) {
        return new FishingHookPredicate(Optional.of(requirement));
    }
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        if (inOpenWater.isEmpty()) {
            return true;
        }
        return entity instanceof net.minecraft.world.entity.projectile.FishingHook hook && hook.isOpenWaterFishing() == inOpenWater.get();
    }
}

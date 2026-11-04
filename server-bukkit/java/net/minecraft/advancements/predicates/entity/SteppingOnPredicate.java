// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record SteppingOnPredicate(LocationPredicate predicate) implements EntitySubPredicate {
    public static final Codec<SteppingOnPredicate> CODEC = LocationPredicate.CODEC.xmap(SteppingOnPredicate::new, SteppingOnPredicate::predicate);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        if (!entity.onGround()) {
            return false;
        }
        Vec3 at = Vec3.atCenterOf(entity.getOnPos());
        return predicate.matches(level, at.x(), at.y(), at.z());
    }
}

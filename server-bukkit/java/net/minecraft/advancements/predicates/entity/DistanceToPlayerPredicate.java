// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record DistanceToPlayerPredicate(DistancePredicate distance) implements EntitySubPredicate {
    public static final Codec<DistanceToPlayerPredicate> CODEC = DistancePredicate.CODEC.xmap(DistanceToPlayerPredicate::new, DistanceToPlayerPredicate::distance);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return position != null && distance.matches(position.x, position.y, position.z, entity.getX(), entity.getY(), entity.getZ());
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record MovementAffectedByPredicate(LocationPredicate predicate) implements EntitySubPredicate {
    public static final Codec<MovementAffectedByPredicate> CODEC = LocationPredicate.CODEC.xmap(MovementAffectedByPredicate::new, MovementAffectedByPredicate::predicate);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        Vec3 at = Vec3.atCenterOf(entity.getBlockPosBelowThatAffectsMyMovement());
        return predicate.matches(level, at.x(), at.y(), at.z());
    }
}

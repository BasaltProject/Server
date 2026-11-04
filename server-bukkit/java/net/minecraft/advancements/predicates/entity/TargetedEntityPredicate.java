// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record TargetedEntityPredicate(EntityPredicate targetedEntity) implements EntitySubPredicate {
    public static final Codec<TargetedEntityPredicate> CODEC = EntityPredicate.CODEC.xmap(TargetedEntityPredicate::new, TargetedEntityPredicate::targetedEntity);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return targetedEntity.matches(level, position, entity instanceof Mob mob ? mob.getTarget() : null);
    }
}

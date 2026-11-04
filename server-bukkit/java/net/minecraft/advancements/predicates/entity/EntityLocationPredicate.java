// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record EntityLocationPredicate(LocationPredicate predicate) implements EntitySubPredicate {
    public static final Codec<EntityLocationPredicate> CODEC = LocationPredicate.CODEC.xmap(EntityLocationPredicate::new, EntityLocationPredicate::predicate);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return predicate.matches(level, entity.getX(), entity.getY(), entity.getZ());
    }
}

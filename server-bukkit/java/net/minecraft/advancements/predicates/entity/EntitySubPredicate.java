// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public interface EntitySubPredicate {
    EntitySubPredicate ALWAYS_TRUE = (entity, level, position) -> true;
    boolean matches(Entity entity, ServerLevel level, Vec3 position);
    default EntitySubPredicate and(EntitySubPredicate other) {
        return (entity, level, position) -> matches(entity, level, position) && other.matches(entity, level, position);
    }
}

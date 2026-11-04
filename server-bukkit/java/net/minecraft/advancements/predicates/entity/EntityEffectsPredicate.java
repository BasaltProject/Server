// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record EntityEffectsPredicate(MobEffectsPredicate effects) implements EntitySubPredicate {
    public static final Codec<EntityEffectsPredicate> CODEC = MobEffectsPredicate.CODEC.xmap(EntityEffectsPredicate::new, EntityEffectsPredicate::effects);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return effects.matches(entity);
    }
}

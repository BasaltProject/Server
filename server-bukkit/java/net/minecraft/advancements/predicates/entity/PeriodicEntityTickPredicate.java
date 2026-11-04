// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record PeriodicEntityTickPredicate(int periodicTick) implements EntitySubPredicate {
    public static final Codec<PeriodicEntityTickPredicate> CODEC = net.minecraft.util.ExtraCodecs.POSITIVE_INT.xmap(PeriodicEntityTickPredicate::new, PeriodicEntityTickPredicate::periodicTick);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return entity.tickCount % periodicTick == 0;
    }
}

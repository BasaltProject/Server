// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record PassengerPredicate(EntityPredicate passenger) implements EntitySubPredicate {
    public static final Codec<PassengerPredicate> CODEC = EntityPredicate.CODEC.xmap(PassengerPredicate::new, PassengerPredicate::passenger);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        for (Entity rider : entity.getPassengers()) {
            if (passenger.matches(level, position, rider)) {
                return true;
            }
        }
        return false;
    }
}

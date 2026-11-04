// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record VehiclePredicate(EntityPredicate vehicle) implements EntitySubPredicate {
    public static final Codec<VehiclePredicate> CODEC = EntityPredicate.CODEC.xmap(VehiclePredicate::new, VehiclePredicate::vehicle);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return vehicle.matches(level, position, entity.getVehicle());
    }
}

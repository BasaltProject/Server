// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record CubeMobPredicate(MinMaxBounds.Ints size) implements EntitySubPredicate {
    public static final Codec<CubeMobPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        MinMaxBounds.Ints.CODEC.optionalFieldOf("size", MinMaxBounds.Ints.ANY).forGetter(CubeMobPredicate::size)).apply(i, CubeMobPredicate::new));
    public static CubeMobPredicate sized(MinMaxBounds.Ints size) {
        return new CubeMobPredicate(size);
    }
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return entity instanceof net.minecraft.world.entity.monster.cubemob.AbstractCubeMob cube && size.matches(cube.getSize());
    }
}

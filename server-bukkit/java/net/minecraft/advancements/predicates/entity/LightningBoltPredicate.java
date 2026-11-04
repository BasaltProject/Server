// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record LightningBoltPredicate(MinMaxBounds.Ints blocksSetOnFire, Optional<EntityPredicate> entityStruck) implements EntitySubPredicate {
    public static final Codec<LightningBoltPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        MinMaxBounds.Ints.CODEC.optionalFieldOf("blocks_set_on_fire", MinMaxBounds.Ints.ANY).forGetter(LightningBoltPredicate::blocksSetOnFire),
        EntityPredicate.CODEC.optionalFieldOf("entity_struck").forGetter(LightningBoltPredicate::entityStruck)).apply(i, LightningBoltPredicate::new));
    public static LightningBoltPredicate blockSetOnFire(MinMaxBounds.Ints count) {
        return new LightningBoltPredicate(count, Optional.empty());
    }
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        if (!(entity instanceof LightningBolt bolt) || !blocksSetOnFire.matches(bolt.getBlocksSetOnFire())) {
            return false;
        }
        return entityStruck.isEmpty() || bolt.getHitEntities().anyMatch(hit -> entityStruck.get().matches(level, position, hit));
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record SheepPredicate(Optional<Boolean> sheared) implements EntitySubPredicate {
    public static final Codec<SheepPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.BOOL.optionalFieldOf("sheared").forGetter(SheepPredicate::sheared)).apply(i, SheepPredicate::new));
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return entity instanceof net.minecraft.world.entity.animal.sheep.Sheep sheep && (sheared.isEmpty() || sheep.isSheared() == sheared.get());
    }
    public static SheepPredicate hasWool() {
        return new SheepPredicate(Optional.of(false));
    }
}

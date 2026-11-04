// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record EntityTagPredicate(Optional<List<String>> anyOf, Optional<List<String>> allOf, Optional<List<String>> noneOf) implements EntitySubPredicate {
    public static final Codec<EntityTagPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.STRING.listOf().optionalFieldOf("any_of").forGetter(EntityTagPredicate::anyOf),
        Codec.STRING.listOf().optionalFieldOf("all_of").forGetter(EntityTagPredicate::allOf),
        Codec.STRING.listOf().optionalFieldOf("none_of").forGetter(EntityTagPredicate::noneOf)).apply(i, EntityTagPredicate::new));
    public boolean matches(Set<String> tags) {
        if (anyOf.isPresent() && anyOf.get().stream().noneMatch(tags::contains)) {
            return false;
        }
        if (noneOf.isPresent() && noneOf.get().stream().anyMatch(tags::contains)) {
            return false;
        }
        return allOf.isEmpty() || tags.containsAll(allOf.get());
    }
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return matches(entity.entityTags());
    }
}

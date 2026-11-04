// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import java.util.Map;
import net.minecraft.core.component.predicates.DataComponentPredicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;
public record EntityPartialComponentsPredicate(Map<DataComponentPredicate.Type<?>, DataComponentPredicate> predicates) implements EntitySubPredicate {
    public static final Codec<EntityPartialComponentsPredicate> CODEC = DataComponentPredicate.CODEC
        .xmap(EntityPartialComponentsPredicate::new, EntityPartialComponentsPredicate::predicates);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        for (DataComponentPredicate predicate : predicates.values()) {
            if (!predicate.matches(entity)) {
                return false;
            }
        }
        return true;
    }
}

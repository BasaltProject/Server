// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import net.minecraft.core.component.DataComponentExactPredicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;
public record EntityExactDataComponentsPredicate(DataComponentExactPredicate predicate) implements EntitySubPredicate {
    public static final Codec<EntityExactDataComponentsPredicate> CODEC = DataComponentExactPredicate.CODEC
        .xmap(EntityExactDataComponentsPredicate::new, EntityExactDataComponentsPredicate::predicate);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return predicate.test(entity);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record EntityNbtPredicate(NbtPredicate nbt) implements EntitySubPredicate {
    public static final Codec<EntityNbtPredicate> CODEC = NbtPredicate.CODEC.xmap(EntityNbtPredicate::new, EntityNbtPredicate::nbt);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return nbt.matches(NbtPredicate.getEntityTagToCompare(entity));
    }
}

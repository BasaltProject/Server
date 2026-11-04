// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record EntityTypePredicate(net.minecraft.core.HolderSet<EntityType<?>> types) implements EntitySubPredicate {
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static final Codec<EntityTypePredicate> CODEC = ((Codec<net.minecraft.core.HolderSet<EntityType<?>>>) (Codec) org.basaltmc.bukkit.facade.ReflectiveCodecs.holderSet(EntityType.class))
        .xmap(EntityTypePredicate::new, EntityTypePredicate::types);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return matches(entity.typeHolder());
    }
    public static EntityTypePredicate of(net.minecraft.core.HolderGetter<EntityType<?>> lookup, EntityType<?> type) {
        return new EntityTypePredicate(net.minecraft.core.HolderSet.direct(type.builtInRegistryHolder()));
    }
    public static EntityTypePredicate of(net.minecraft.core.HolderGetter<EntityType<?>> lookup, net.minecraft.tags.TagKey<EntityType<?>> type) {
        return new EntityTypePredicate(lookup.getOrThrow(type));
    }
    public boolean matches(net.minecraft.core.Holder<EntityType<?>> type) {
        return org.basaltmc.bukkit.facade.HolderSets.contains(types, type);
    }
}

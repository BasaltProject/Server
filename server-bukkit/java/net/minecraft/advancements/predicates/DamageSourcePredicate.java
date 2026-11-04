// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.phys.Vec3;
public record DamageSourcePredicate(List<TagPredicate<DamageType>> tags, Optional<EntityPredicate> directEntity, Optional<EntityPredicate> sourceEntity, Optional<Boolean> isDirect) {
    public static final Codec<DamageSourcePredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        TagPredicate.<DamageType>codec((net.minecraft.resources.ResourceKey<? extends net.minecraft.core.Registry<DamageType>>) (net.minecraft.resources.ResourceKey) Registries.DAMAGE_TYPE).listOf().optionalFieldOf("tags", List.of()).forGetter(DamageSourcePredicate::tags),
        EntityPredicate.CODEC.optionalFieldOf("direct_entity").forGetter(DamageSourcePredicate::directEntity),
        EntityPredicate.CODEC.optionalFieldOf("source_entity").forGetter(DamageSourcePredicate::sourceEntity),
        Codec.BOOL.optionalFieldOf("is_direct").forGetter(DamageSourcePredicate::isDirect)).apply(i, DamageSourcePredicate::new));
    public boolean matches(ServerPlayer player, DamageSource source) {
        return matches(player.level(), player.position(), source);
    }
    public boolean matches(ServerLevel level, Vec3 position, DamageSource source) {
        for (TagPredicate<DamageType> tag : tags) {
            if (!tag.matches(source.typeHolder())) {
                return false;
            }
        }
        if (directEntity.isPresent() && !directEntity.get().matches(level, position, source.getDirectEntity())) {
            return false;
        }
        if (sourceEntity.isPresent() && !sourceEntity.get().matches(level, position, source.getEntity())) {
            return false;
        }
        return isDirect.isEmpty() || isDirect.get() == source.isDirect();
    }
    public static class Builder {
        private final List<TagPredicate<DamageType>> tags = new ArrayList<>();
        private Optional<EntityPredicate> directEntity = Optional.empty();
        private Optional<EntityPredicate> sourceEntity = Optional.empty();
        private Optional<Boolean> isDirect = Optional.empty();
        public static Builder damageType() {
            return new Builder();
        }
        public Builder tag(TagPredicate<DamageType> tag) {
            tags.add(tag);
            return this;
        }
        public Builder direct(EntityPredicate.Builder entity) {
            directEntity = Optional.of(entity.build());
            return this;
        }
        public Builder source(EntityPredicate.Builder entity) {
            sourceEntity = Optional.of(entity.build());
            return this;
        }
        public Builder isDirect(Boolean direct) {
            isDirect = Optional.of(direct);
            return this;
        }
        public DamageSourcePredicate build() {
            return new DamageSourcePredicate(List.copyOf(tags), directEntity, sourceEntity, isDirect);
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public record LootItemEntityPropertyCondition(Optional<EntityPredicate> predicate, LootContext.EntityTarget entityTarget) implements LootItemCondition, org.basaltmc.bukkit.loot.LootJson.Node {
    public static final MapCodec<LootItemEntityPropertyCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.LootItemEntityPropertyCondition.class, "MAP_CODEC");
    public void writeJson(StringBuilder out) {
        out.append("{\"type\":\"minecraft:entity_properties\",\"entity\":\"")
            .append(entityTarget.getSerializedName()).append("\",\"predicate\":");
        if (predicate.isPresent()) {
            out.append(EntityPredicate.CODEC.encodeStart(com.mojang.serialization.JsonOps.INSTANCE, predicate.get())
                .getOrThrow(message -> new IllegalStateException("Cannot write entity predicate: " + message)));
        } else {
            out.append("{}");
        }
        out.append('}');
    }
    public MapCodec<LootItemEntityPropertyCondition> codec() {
        return MAP_CODEC;
    }
    public java.util.Set<net.minecraft.util.context.ContextKey<?>> getReferencedContextParams() {
        return java.util.Set.of(entityTarget.contextParam());
    }
    public boolean test(LootContext context) {
        Entity entity = context.getOptional(entityTarget.contextParam());
        net.minecraft.world.phys.Vec3 origin = context.getOptional(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN);
        if (entity == null || origin == null) {
            return false;
        }
        return predicate.isEmpty() || predicate.get().matches(context.getLevel(), origin, entity);
    }
    public void validate(ValidationContext context) {
        return;
    }
    public static LootItemCondition.Builder entityPresent(LootContext.EntityTarget target) {
        return hasProperties(target, EntityPredicate.Builder.entity());
    }
    public static LootItemCondition.Builder hasProperties(LootContext.EntityTarget target, EntityPredicate.Builder predicate) {
        return () -> new LootItemEntityPropertyCondition(Optional.of(predicate.build()), target);
    }
    public static LootItemCondition.Builder hasProperties(LootContext.EntityTarget target, EntityPredicate predicate) {
        return () -> new LootItemEntityPropertyCondition(Optional.of(predicate), target);
    }
}

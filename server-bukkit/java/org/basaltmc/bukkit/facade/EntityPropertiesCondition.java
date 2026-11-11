package org.basaltmc.bukkit.facade;

import com.mojang.serialization.MapCodec;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;

/** The `entity_properties` loot condition on the looted entity, as advancement triggers use it for their player. */
public record EntityPropertiesCondition(EntityPredicate predicate) implements LootItemCondition {
    @Override
    public MapCodec<? extends LootItemCondition> codec() {
        return MapCodec.unit(this);
    }

    @Override
    public boolean test(LootContext context) {
        net.minecraft.world.entity.Entity entity = context.getOptional(LootContextParams.THIS_ENTITY);
        net.minecraft.world.phys.Vec3 origin = context.getOptional(LootContextParams.ORIGIN);
        return entity != null && predicate.matches(context.getLevel(), origin == null ? entity.position() : origin, entity);
    }
}

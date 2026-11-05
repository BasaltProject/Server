// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.parameters;

import com.mojang.serialization.Codec;
import net.minecraft.core.Registry;
import net.minecraft.util.context.ContextKey;
import net.minecraft.util.context.ContextKeySet;

public class LootContextParamSets {
    public static final Codec<ContextKeySet> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(LootContextParamSets.class, "CODEC");

    private static ContextKeySet of(ContextKey<?>[] required, ContextKey<?>... optional) {
        ContextKeySet.Builder builder = new ContextKeySet.Builder();
        for (ContextKey<?> key : required) {
            builder.required(key);
        }
        for (ContextKey<?> key : optional) {
            builder.optional(key);
        }
        return builder.build();
    }

    private static ContextKey<?>[] keys(ContextKey<?>... keys) {
        return keys;
    }

    public static final ContextKeySet EMPTY = of(keys());
    public static final ContextKeySet ALL_PARAMS = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.LAST_DAMAGE_PLAYER, LootContextParams.DAMAGE_SOURCE, LootContextParams.ATTACKING_ENTITY, LootContextParams.DIRECT_ATTACKING_ENTITY, LootContextParams.ORIGIN, LootContextParams.BLOCK_STATE, LootContextParams.BLOCK_ENTITY, LootContextParams.TOOL, LootContextParams.EXPLOSION_RADIUS, LootContextParams.ADDITIONAL_COST_COMPONENT_ALLOWED, LootContextParams.CONTAINER, LootContextParams.INTERACTING_ENTITY, LootContextParams.TARGET_ENTITY, LootContextParams.ENCHANTMENT_ACTIVE, LootContextParams.ENCHANTMENT_LEVEL));
    public static final ContextKeySet CHEST = of(keys(LootContextParams.ORIGIN), LootContextParams.THIS_ENTITY);
    public static final ContextKeySet COMMAND = of(keys(LootContextParams.ORIGIN), LootContextParams.THIS_ENTITY);
    public static final ContextKeySet COMMAND_SLOT_SOURCE = of(keys(LootContextParams.ORIGIN, LootContextParams.CONTAINER), LootContextParams.THIS_ENTITY);
    public static final ContextKeySet COMMAND_COMPUTE_DEFAULT = of(keys(LootContextParams.ORIGIN), LootContextParams.THIS_ENTITY);
    public static final ContextKeySet COMMAND_COMPUTE_POSITION = of(keys(LootContextParams.ORIGIN, LootContextParams.BLOCK_STATE), LootContextParams.THIS_ENTITY, LootContextParams.BLOCK_ENTITY);
    public static final ContextKeySet COMMAND_COMPUTE_ENTITY = of(keys(LootContextParams.ORIGIN, LootContextParams.TARGET_ENTITY), LootContextParams.THIS_ENTITY);
    public static final ContextKeySet SELECTOR = of(keys(LootContextParams.ORIGIN, LootContextParams.THIS_ENTITY));
    public static final ContextKeySet VILLAGER_TRADE = of(keys(LootContextParams.ORIGIN, LootContextParams.THIS_ENTITY, LootContextParams.ADDITIONAL_COST_COMPONENT_ALLOWED));
    public static final ContextKeySet FISHING = of(keys(LootContextParams.ORIGIN, LootContextParams.TOOL), LootContextParams.THIS_ENTITY);
    public static final ContextKeySet ENTITY = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ORIGIN, LootContextParams.DAMAGE_SOURCE), LootContextParams.ATTACKING_ENTITY, LootContextParams.DIRECT_ATTACKING_ENTITY, LootContextParams.LAST_DAMAGE_PLAYER);
    public static final ContextKeySet EQUIPMENT = of(keys(LootContextParams.ORIGIN, LootContextParams.THIS_ENTITY));
    public static final ContextKeySet ARCHAEOLOGY = of(keys(LootContextParams.ORIGIN, LootContextParams.THIS_ENTITY, LootContextParams.TOOL));
    public static final ContextKeySet GIFT = of(keys(LootContextParams.ORIGIN, LootContextParams.THIS_ENTITY));
    public static final ContextKeySet PIGLIN_BARTER = of(keys(LootContextParams.THIS_ENTITY));
    public static final ContextKeySet VAULT = of(keys(LootContextParams.ORIGIN), LootContextParams.THIS_ENTITY, LootContextParams.TOOL);
    public static final ContextKeySet ADVANCEMENT_REWARD = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ORIGIN));
    public static final ContextKeySet ADVANCEMENT_ENTITY = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ORIGIN));
    public static final ContextKeySet ADVANCEMENT_LOCATION = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ORIGIN, LootContextParams.TOOL, LootContextParams.BLOCK_STATE), LootContextParams.BLOCK_ENTITY);
    public static final ContextKeySet BLOCK_USE = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ORIGIN, LootContextParams.BLOCK_STATE));
    public static final ContextKeySet BLOCK = of(keys(LootContextParams.BLOCK_STATE, LootContextParams.ORIGIN, LootContextParams.TOOL), LootContextParams.THIS_ENTITY, LootContextParams.BLOCK_ENTITY, LootContextParams.EXPLOSION_RADIUS);
    public static final ContextKeySet SHEARING = of(keys(LootContextParams.ORIGIN, LootContextParams.THIS_ENTITY, LootContextParams.TOOL));
    public static final ContextKeySet ENTITY_INTERACT = of(keys(LootContextParams.TARGET_ENTITY, LootContextParams.TOOL), LootContextParams.INTERACTING_ENTITY);
    public static final ContextKeySet BLOCK_INTERACT = of(keys(LootContextParams.BLOCK_STATE, LootContextParams.ORIGIN), LootContextParams.BLOCK_ENTITY, LootContextParams.INTERACTING_ENTITY, LootContextParams.TOOL);
    public static final ContextKeySet CONTAINER_PROCESS = of(keys(LootContextParams.BLOCK_ENTITY, LootContextParams.BLOCK_STATE, LootContextParams.CONTAINER, LootContextParams.ORIGIN));
    public static final ContextKeySet ENCHANTED_DAMAGE = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ENCHANTMENT_LEVEL, LootContextParams.ORIGIN, LootContextParams.DAMAGE_SOURCE), LootContextParams.DIRECT_ATTACKING_ENTITY, LootContextParams.ATTACKING_ENTITY);
    public static final ContextKeySet ENCHANTED_ITEM = of(keys(LootContextParams.TOOL, LootContextParams.ENCHANTMENT_LEVEL));
    public static final ContextKeySet ENCHANTED_LOCATION = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ENCHANTMENT_LEVEL, LootContextParams.ORIGIN, LootContextParams.ENCHANTMENT_ACTIVE));
    public static final ContextKeySet ENCHANTED_ENTITY = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ENCHANTMENT_LEVEL, LootContextParams.ORIGIN));
    public static final ContextKeySet HIT_BLOCK = of(keys(LootContextParams.THIS_ENTITY, LootContextParams.ENCHANTMENT_LEVEL, LootContextParams.ORIGIN, LootContextParams.BLOCK_STATE));

    public static ContextKeySet bootstrap(Registry<ContextKeySet> registry) {
        return ALL_PARAMS;
    }

    public static void validate() {
        java.util.Set<ContextKey<?>> declared = new java.util.HashSet<>();
        for (ContextKeySet set : java.util.List.of(CHEST, COMMAND, SELECTOR, ENTITY, BLOCK, FISHING, VAULT, BLOCK_INTERACT, ENCHANTED_DAMAGE)) {
            declared.addAll(set.allowed());
        }
        if (!ALL_PARAMS.required().containsAll(declared)) {
            throw new IllegalStateException("Missing parameters from 'all_params'");
        }
    }
}

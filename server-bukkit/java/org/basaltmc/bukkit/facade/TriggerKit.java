package org.basaltmc.bukkit.facade;

import com.mojang.serialization.MapCodec;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;

/** Shared plumbing of the advancement trigger instances: condition codecs, condition tests and validation. */
public final class TriggerKit {
    public static MapCodec<Optional<Holder<LootItemCondition>>> cond(String name) {
        return PlayerConditions.CODEC.optionalFieldOf(name, Optional.empty());
    }

    public static MapCodec<List<Holder<LootItemCondition>>> conds(String name) {
        return PlayerConditions.CODEC.listOf().optionalFieldOf(name, List.of()).xmap(
            lists -> lists.stream().flatMap(o -> o.stream()).toList(), list -> list.stream().map(Optional::of).toList());
    }

    public static <T> Optional<T> none() {
        return Optional.empty();
    }

    public static LootContext blockContext(net.minecraft.server.level.ServerPlayer player, net.minecraft.core.BlockPos pos,
        net.minecraft.world.item.ItemInstance tool) {
        var level = player.level();
        var params = new net.minecraft.world.level.storage.loot.LootParams.Builder(level);
        params.withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN, net.minecraft.world.phys.Vec3.atCenterOf(pos));
        params.withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.THIS_ENTITY, player);
        params.withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.BLOCK_STATE, level.getBlockState(pos));
        if (tool == null) {
            return new LootContext.Builder(params.create(net.minecraft.world.level.storage.loot.parameters.LootContextParamSets.BLOCK_USE)).create(Optional.empty());
        }
        params.withOptionalParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.BLOCK_ENTITY, level.getBlockEntity(pos));
        params.withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.TOOL, tool);
        return new LootContext.Builder(params.create(net.minecraft.world.level.storage.loot.parameters.LootContextParamSets.ADVANCEMENT_LOCATION)).create(Optional.empty());
    }

    public static void fireNative(net.minecraft.advancements.triggers.CriterionTrigger<?> trigger, net.minecraft.server.level.ServerPlayer player,
        net.minecraft.core.BlockPos pos, Object item, double distance) {
        net.minecraft.world.item.ItemStack stack = item instanceof net.minecraft.world.item.ItemStack held ? held : net.minecraft.world.item.ItemStack.EMPTY;
        String name = stack.isEmpty() ? null : net.minecraft.core.registries.BuiltInRegistries.ITEM.getKey(stack.getItem()).toString();
        String id = net.minecraft.advancements.triggers.CriteriaTriggers.idOf(trigger).toString();
        org.basaltmc.bukkit.MiscNatives.advTrigger(player.getUUID().toString(), id, pos != null, pos == null ? 0 : pos.getX(),
            pos == null ? 0 : pos.getY(), pos == null ? 0 : pos.getZ(), name, stack.getCount(), distance);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static com.mojang.serialization.Codec<net.minecraft.core.HolderSet<net.minecraft.world.item.crafting.Recipe<?>>> recipeSet() {
        return (com.mojang.serialization.Codec) ReflectiveCodecs.holderSet(net.minecraft.core.registries.Registries.RECIPE);
    }

    public static boolean passes(Optional<Holder<LootItemCondition>> condition, LootContext context) {
        return condition.isEmpty() || (context != null && condition.get().value().test(context));
    }

    public static boolean passesAll(List<Holder<LootItemCondition>> conditions, Collection<? extends LootContext> contexts) {
        if (conditions.isEmpty()) {
            return true;
        }
        for (Holder<LootItemCondition> condition : conditions) {
            if (contexts.stream().noneMatch(condition.value()::test)) {
                return false;
            }
        }
        return true;
    }

    public static void check(ValidationContextSource source, String name, Optional<Holder<LootItemCondition>> condition) {
        Validatable.validateHolder(source.entityContext(), name, condition);
    }

    public static void checkAll(ValidationContextSource source, String name, List<Holder<LootItemCondition>> conditions) {
        Validatable.validateHolder(source.entityContext(), name, conditions);
    }

    private TriggerKit() {}
}

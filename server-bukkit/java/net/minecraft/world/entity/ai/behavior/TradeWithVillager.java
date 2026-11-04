// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.phys.Vec3;

public class TradeWithVillager extends Behavior<Villager> {
    private Set<Item> trades = Set.of();

    public TradeWithVillager() {
        super(Map.of(MemoryModuleType.INTERACTION_TARGET, MemoryStatus.VALUE_PRESENT, MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES, MemoryStatus.VALUE_PRESENT));
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        return BehaviorUtils.targetIsValid(body.getBrain(), MemoryModuleType.INTERACTION_TARGET, net.minecraft.world.entity.EntityTypes.VILLAGER);
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return checkExtraStartConditions(level, body);
    }

    protected void start(ServerLevel level, Villager me, long timestamp) {
        Villager target = (Villager) me.getBrain().getMemory(MemoryModuleType.INTERACTION_TARGET).get();
        BehaviorUtils.lockGazeAndWalkToEachOther(me, target, 0.5F, 2);
        Set<Item> targetItems = target.getVillagerData().profession().value().requestedItems();
        Set<Item> selfItems = me.getVillagerData().profession().value().requestedItems();
        trades = targetItems.stream().filter(item -> !selfItems.contains(item)).collect(java.util.stream.Collectors.toSet());
    }

    protected void tick(ServerLevel level, Villager body, long timestamp) {
        Villager target = (Villager) body.getBrain().getMemory(MemoryModuleType.INTERACTION_TARGET).get();
        if (body.distanceToSqr(target) > 5.0) {
            return;
        }
        BehaviorUtils.lockGazeAndWalkToEachOther(body, target, 0.5F, 2);
        body.gossip(level, target, timestamp);
        boolean isFarmer = body.getVillagerData().profession().is(net.minecraft.world.entity.npc.villager.VillagerProfession.FARMER);
        if (body.hasExcessFood() && (isFarmer || target.wantsMoreFood())) {
            throwHalfStack(body, stack -> stack.has(net.minecraft.core.component.DataComponents.VILLAGER_FOOD), target);
        }
        if (isFarmer && body.getInventory().countItem(Items.WHEAT) > Items.WHEAT.getDefaultMaxStackSize() / 2) {
            throwHalfStack(body, stack -> stack.is(Items.WHEAT), target);
        }
        if (!trades.isEmpty() && body.getInventory().hasAnyOf(trades)) {
            throwHalfStack(body, stack -> trades.contains(stack.getItem()), target);
        }
    }

    protected void stop(ServerLevel level, Villager body, long timestamp) {
        body.getBrain().eraseMemory(MemoryModuleType.INTERACTION_TARGET);
    }

    private static void throwHalfStack(Villager villager, java.util.function.Predicate<ItemStack> predicate, LivingEntity target) {
        var inventory = villager.getInventory();
        ItemStack toThrow = ItemStack.EMPTY;
        for (int i = 0; i < inventory.getContainerSize(); ++i) {
            ItemStack stack = inventory.getItem(i);
            if (stack.isEmpty() || !predicate.test(stack)) {
                continue;
            }
            int count;
            if (stack.getCount() > stack.getMaxStackSize() / 2) {
                count = stack.getCount() / 2;
            } else if (stack.getCount() > 24) {
                count = stack.getCount() - 24;
            } else {
                continue;
            }
            toThrow = stack.split(count);
            break;
        }
        if (!toThrow.isEmpty()) {
            BehaviorUtils.throwItem(villager, toThrow, target.position());
        }
    }
}

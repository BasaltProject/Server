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

public class ShowTradesToPlayer extends Behavior<Villager> {
    private static final int MAX_LOOK_TIME = 900;
    private static final int STARTING_LOOK_TIME = 40;
    private ItemStack playerItemStack;
    private final List<ItemStack> displayItems = new java.util.ArrayList<>();
    private int cycleCounter;
    private int displayIndex;
    private int lookTime;

    public ShowTradesToPlayer(int minDuration, int maxDuration) {
        super(Map.of(MemoryModuleType.INTERACTION_TARGET, MemoryStatus.VALUE_PRESENT), minDuration, maxDuration);
    }

    public boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        Optional<LivingEntity> target = body.getBrain().getMemory(MemoryModuleType.INTERACTION_TARGET);
        return target.isPresent() && target.get().is(net.minecraft.world.entity.EntityTypes.PLAYER) && body.isAlive() && target.get().isAlive() && !body.isBaby()
            && body.distanceToSqr(target.get()) <= 17.0;
    }

    public boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return checkExtraStartConditions(level, body) && lookTime > 0 && body.getBrain().getMemory(MemoryModuleType.INTERACTION_TARGET).isPresent();
    }

    public void start(ServerLevel level, Villager body, long timestamp) {
        super.start(level, body, timestamp);
        lookAtTarget(body);
        cycleCounter = 0;
        displayIndex = 0;
        lookTime = STARTING_LOOK_TIME;
    }

    public void tick(ServerLevel level, Villager body, long timestamp) {
        LivingEntity target = lookAtTarget(body);
        findItemsToDisplay(target, body);
        if (!displayItems.isEmpty()) {
            displayCyclingItems(body);
        } else {
            clearHeldItem(body);
            lookTime = Math.min(lookTime, STARTING_LOOK_TIME);
        }
        --lookTime;
    }

    public void stop(ServerLevel level, Villager body, long timestamp) {
        super.stop(level, body, timestamp);
        body.getBrain().eraseMemory(MemoryModuleType.INTERACTION_TARGET);
        clearHeldItem(body);
        playerItemStack = null;
    }

    private void findItemsToDisplay(LivingEntity player, Villager villager) {
        ItemStack current = player.getMainHandItem();
        if (playerItemStack != null && ItemStack.isSameItem(playerItemStack, current)) {
            return;
        }
        playerItemStack = current;
        displayItems.clear();
        if (!playerItemStack.isEmpty()) {
            for (var offer : villager.getOffers()) {
                if (!offer.isOutOfStock() && (ItemStack.isSameItem(playerItemStack, offer.getCostA()) || ItemStack.isSameItem(playerItemStack, offer.getCostB()))) {
                    displayItems.add(offer.assemble());
                }
            }
            if (!displayItems.isEmpty()) {
                lookTime = MAX_LOOK_TIME;
                displayAsHeldItem(villager, displayItems.get(0));
            }
        }
    }

    private static void clearHeldItem(Villager body) {
        body.setItemSlot(EquipmentSlot.MAINHAND, ItemStack.EMPTY);
        body.setDropChance(EquipmentSlot.MAINHAND, 0.085F);
    }

    private static void displayAsHeldItem(Villager body, ItemStack itemStack) {
        body.setItemSlot(EquipmentSlot.MAINHAND, itemStack);
        body.setDropChance(EquipmentSlot.MAINHAND, 0.0F);
    }

    private LivingEntity lookAtTarget(Villager body) {
        LivingEntity target = body.getBrain().getMemory(MemoryModuleType.INTERACTION_TARGET).get();
        body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(target, true));
        return target;
    }

    private void displayCyclingItems(Villager villager) {
        if (displayItems.size() >= 2 && ++cycleCounter >= 40) {
            ++displayIndex;
            cycleCounter = 0;
            if (displayIndex > displayItems.size() - 1) {
                displayIndex = 0;
            }
            displayAsHeldItem(villager, displayItems.get(displayIndex));
        }
    }
}

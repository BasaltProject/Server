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

public class UseBonemeal extends Behavior<Villager> {
    private static final int BONEMEALING_DURATION = 80;
    private long nextWorkCycleTime;
    private long lastBonemealingSession;
    private int timeWorkedSoFar;
    private Optional<BlockPos> cropPos = Optional.empty();

    public UseBonemeal() {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.VALUE_ABSENT, MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT));
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        if (body.tickCount % 10 != 0 || (lastBonemealingSession != 0L && lastBonemealingSession + 160L > body.tickCount)) {
            return false;
        }
        if (body.getInventory().countItem(Items.BONE_MEAL) <= 0) {
            return false;
        }
        cropPos = pickNextTarget(level, body);
        return cropPos.isPresent();
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return timeWorkedSoFar < BONEMEALING_DURATION && cropPos.isPresent();
    }

    private Optional<BlockPos> pickNextTarget(ServerLevel level, Villager body) {
        net.minecraft.util.RandomSource random = level.getRandom();
        Optional<BlockPos> result = Optional.empty();
        int count = 0;
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                for (int z = -1; z <= 1; ++z) {
                    pos.setWithOffset(body.blockPosition(), x, y, z);
                    if (validPos(pos, level) && random.nextInt(++count) == 0) {
                        result = Optional.of(pos.immutable());
                    }
                }
            }
        }
        return result;
    }

    private boolean validPos(BlockPos pos, ServerLevel level) {
        var state = level.getBlockState(pos);
        return state.getBlock() instanceof net.minecraft.world.level.block.CropBlock crop && !crop.isMaxAge(state);
    }

    protected void start(ServerLevel level, Villager body, long timestamp) {
        setCurrentCropAsTarget(body);
        body.setItemSlot(EquipmentSlot.MAINHAND, new ItemStack(Items.BONE_MEAL));
        nextWorkCycleTime = timestamp;
        timeWorkedSoFar = 0;
    }

    private void setCurrentCropAsTarget(Villager body) {
        cropPos.ifPresent(pos -> {
            BlockPosTracker tracker = new BlockPosTracker(pos);
            body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, tracker);
            body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(tracker, 0.5F, 1));
        });
    }

    protected void stop(ServerLevel level, Villager body, long timestamp) {
        body.setItemSlot(EquipmentSlot.MAINHAND, ItemStack.EMPTY);
        lastBonemealingSession = body.tickCount;
    }

    protected void tick(ServerLevel level, Villager body, long timestamp) {
        BlockPos target = cropPos.get();
        if (timestamp >= nextWorkCycleTime && target.closerToCenterThan(body.position(), 1.0)) {
            var inventory = body.getInventory();
            ItemStack bonemeal = ItemStack.EMPTY;
            for (int i = 0; i < inventory.getContainerSize(); ++i) {
                if (inventory.getItem(i).is(Items.BONE_MEAL)) {
                    bonemeal = inventory.getItem(i);
                    break;
                }
            }
            if (!bonemeal.isEmpty() && net.minecraft.world.item.BoneMealItem.growCrop(bonemeal, level, target)) {
                level.levelEvent(1505, target, 15);
                cropPos = pickNextTarget(level, body);
                setCurrentCropAsTarget(body);
                nextWorkCycleTime = timestamp + 40L;
            }
            ++timeWorkedSoFar;
        }
    }
}

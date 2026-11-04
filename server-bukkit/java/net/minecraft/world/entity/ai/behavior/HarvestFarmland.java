// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import com.google.common.collect.ImmutableList;
import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.Set;
import java.util.Map;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.ai.behavior.*;
import net.minecraft.world.entity.ai.behavior.declarative.BehaviorBuilder;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.util.Unit;
import net.minecraft.world.entity.ai.behavior.declarative.Trigger;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.npc.villager.VillagerProfession;
import net.minecraft.world.level.block.CropBlock;
import net.minecraft.world.level.block.FarmlandBlock;

public class HarvestFarmland extends Behavior<Villager> {
    private static final int HARVEST_DURATION = 200;
    public static final float SPEED_MODIFIER = 0.5F;
    private BlockPos aboveFarmlandPos;
    private long nextOkStartTime;
    private int timeWorkedSoFar;
    private final List<BlockPos> validFarmlandAroundVillager = new ArrayList<>();

    public HarvestFarmland() {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.VALUE_ABSENT, MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.SECONDARY_JOB_SITE, MemoryStatus.VALUE_PRESENT));
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        if (!level.getGameRules().get(net.minecraft.world.level.gamerules.GameRules.MOB_GRIEFING) || !body.getVillagerData().profession().is(VillagerProfession.FARMER)) {
            return false;
        }
        BlockPos.MutableBlockPos pos = body.blockPosition().mutable();
        validFarmlandAroundVillager.clear();
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                for (int z = -1; z <= 1; ++z) {
                    pos.set(body.getX() + x, body.getY() + y, body.getZ() + z);
                    if (validPos(pos, level)) {
                        validFarmlandAroundVillager.add(pos.immutable());
                    }
                }
            }
        }
        aboveFarmlandPos = getValidFarmland(level);
        return aboveFarmlandPos != null;
    }

    private BlockPos getValidFarmland(ServerLevel level) {
        return validFarmlandAroundVillager.isEmpty() ? null : validFarmlandAroundVillager.get(level.getRandom().nextInt(validFarmlandAroundVillager.size()));
    }

    private boolean validPos(BlockPos pos, ServerLevel level) {
        var state = level.getBlockState(pos);
        if (state.getBlock() instanceof CropBlock crop && crop.isMaxAge(state)) {
            return true;
        }
        return state.isAir() && level.getBlockState(pos.below()).getBlock() instanceof FarmlandBlock;
    }

    protected void start(ServerLevel level, Villager body, long timestamp) {
        if (timestamp > nextOkStartTime && aboveFarmlandPos != null) {
            body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(aboveFarmlandPos));
            body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new BlockPosTracker(aboveFarmlandPos), SPEED_MODIFIER, 1));
        }
    }

    protected void stop(ServerLevel level, Villager body, long timestamp) {
        body.getBrain().eraseMemory(MemoryModuleType.LOOK_TARGET);
        body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
        timeWorkedSoFar = 0;
        nextOkStartTime = timestamp + 40L;
    }

    protected void tick(ServerLevel level, Villager body, long timestamp) {
        if (aboveFarmlandPos != null && !aboveFarmlandPos.closerToCenterThan(body.position(), 1.0)) {
            return;
        }
        if (aboveFarmlandPos != null && timestamp > nextOkStartTime) {
            var state = level.getBlockState(aboveFarmlandPos);
            var block = state.getBlock();
            var below = level.getBlockState(aboveFarmlandPos.below()).getBlock();
            if (block instanceof CropBlock crop && crop.isMaxAge(state)) {
                level.destroyBlock(aboveFarmlandPos, true, body);
            }
            if (state.isAir() && below instanceof FarmlandBlock && body.hasFarmSeeds()) {
                plantSeed(level, body);
            }
            if (block instanceof CropBlock crop && !crop.isMaxAge(state)) {
                validFarmlandAroundVillager.remove(aboveFarmlandPos);
                aboveFarmlandPos = getValidFarmland(level);
                if (aboveFarmlandPos != null) {
                    nextOkStartTime = timestamp + 20L;
                    body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new BlockPosTracker(aboveFarmlandPos), SPEED_MODIFIER, 1));
                    body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(aboveFarmlandPos));
                }
            }
        }
        ++timeWorkedSoFar;
    }

    private void plantSeed(ServerLevel level, Villager body) {
        var inventory = body.getInventory();
        for (int i = 0; i < inventory.getContainerSize(); ++i) {
            var stack = inventory.getItem(i);
            if (!stack.isEmpty() && stack.is(net.minecraft.tags.ItemTags.VILLAGER_PLANTABLE_SEEDS) && stack.getItem() instanceof net.minecraft.world.item.BlockItem blockItem) {
                var place = blockItem.getBlock().defaultBlockState();
                level.setBlockAndUpdate(aboveFarmlandPos, place);
                level.gameEvent(net.minecraft.world.level.gameevent.GameEvent.BLOCK_PLACE, aboveFarmlandPos, net.minecraft.world.level.gameevent.GameEvent.Context.of(body, place));
                level.playSound(null, aboveFarmlandPos.getX(), aboveFarmlandPos.getY(), aboveFarmlandPos.getZ(), net.minecraft.sounds.SoundEvents.CROP_PLANTED,
                    net.minecraft.sounds.SoundSource.BLOCKS, 1.0F, 1.0F);
                stack.shrink(1);
                if (stack.isEmpty()) {
                    inventory.setItem(i, net.minecraft.world.item.ItemStack.EMPTY);
                }
                return;
            }
        }
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return timeWorkedSoFar < HARVEST_DURATION;
    }
}

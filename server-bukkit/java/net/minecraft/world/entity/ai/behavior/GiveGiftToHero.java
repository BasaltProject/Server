// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;

public class GiveGiftToHero extends Behavior<Villager> {
    private static final int THROW_GIFT_AT_DISTANCE = 5;
    private static final int MIN_TIME_BETWEEN_GIFTS = 600;
    private static final int TIME_TO_DELAY_FOR_HEAD_TO_FINISH_TURNING = 20;
    private static final float SPEED_MODIFIER = 0.5F;
    private int timeUntilNextGift = MIN_TIME_BETWEEN_GIFTS;
    private boolean giftGivenDuringThisRun;
    private long timeSinceStart;

    public GiveGiftToHero(int timeout) {
        super(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED,
            MemoryModuleType.INTERACTION_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.NEAREST_VISIBLE_PLAYER, MemoryStatus.VALUE_PRESENT), timeout);
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        if (!isHeroVisible(body)) {
            return false;
        }
        if (timeUntilNextGift > 0) {
            --timeUntilNextGift;
            return false;
        }
        return true;
    }

    protected void start(ServerLevel level, Villager body, long timestamp) {
        giftGivenDuringThisRun = false;
        timeSinceStart = timestamp;
        Player player = getNearestTargetableHero(body).get();
        body.getBrain().setMemory(MemoryModuleType.INTERACTION_TARGET, player);
        BehaviorUtils.lookAtEntity(body, player);
    }

    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return isHeroVisible(body) && !giftGivenDuringThisRun;
    }

    protected void tick(ServerLevel level, Villager villager, long timestamp) {
        Player player = getNearestTargetableHero(villager).get();
        BehaviorUtils.lookAtEntity(villager, player);
        if (villager.blockPosition().closerThan(player.blockPosition(), THROW_GIFT_AT_DISTANCE)) {
            if (timestamp - timeSinceStart > TIME_TO_DELAY_FOR_HEAD_TO_FINISH_TURNING) {
                throwGift(villager, player);
                giftGivenDuringThisRun = true;
            }
        } else {
            BehaviorUtils.setWalkAndLookTargetMemories(villager, player, SPEED_MODIFIER, THROW_GIFT_AT_DISTANCE);
        }
    }

    protected void stop(ServerLevel level, Villager body, long timestamp) {
        timeUntilNextGift = MIN_TIME_BETWEEN_GIFTS + level.getRandom().nextInt(6001);
        body.getBrain().eraseMemory(MemoryModuleType.INTERACTION_TARGET);
        body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
        body.getBrain().eraseMemory(MemoryModuleType.LOOK_TARGET);
    }

    private void throwGift(Villager villager, LivingEntity target) {
        if (!(villager.live() instanceof org.bukkit.entity.Villager api)) {
            return;
        }
        org.bukkit.loot.LootTable table = org.bukkit.Bukkit.getLootTable(org.bukkit.NamespacedKey.minecraft(giftTable(villager)));
        if (table == null) {
            return;
        }
        org.bukkit.loot.LootContext context = new org.bukkit.loot.LootContext.Builder(api.getLocation()).lootedEntity(api).build();
        for (org.bukkit.inventory.ItemStack stack : table.populateLoot(new java.util.Random(villager.level().getRandom().nextLong()), context)) {
            BehaviorUtils.throwItem(villager, org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(stack), target.position());
        }
    }

    private static String giftTable(Villager villager) {
        if (villager.isBaby()) {
            return "gameplay/hero_of_the_village/baby_gift";
        }
        String profession = villager.getVillagerData().profession().unwrapKey().map(k -> k.identifier().getPath()).orElse("none");
        return switch (profession) {
            case "armorer", "butcher", "cartographer", "cleric", "farmer", "fisherman", "fletcher", "leatherworker", "librarian", "mason", "shepherd", "toolsmith",
                "weaponsmith" -> "gameplay/hero_of_the_village/" + profession + "_gift";
            default -> "gameplay/hero_of_the_village/unemployed_gift";
        };
    }

    private boolean isHeroVisible(Villager body) {
        return getNearestTargetableHero(body).isPresent();
    }

    private Optional<Player> getNearestTargetableHero(Villager body) {
        return body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_PLAYER).filter(p -> p.hasEffect(net.minecraft.world.effect.MobEffects.HERO_OF_THE_VILLAGE));
    }
}

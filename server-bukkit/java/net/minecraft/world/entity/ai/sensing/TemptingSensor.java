// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Comparator;
import java.util.List;
import java.util.Set;
import java.util.function.BiPredicate;
import java.util.function.Predicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class TemptingSensor extends Sensor<PathfinderMob> {
    private final BiPredicate<PathfinderMob, ItemStack> temptations;

    public TemptingSensor(Predicate<ItemStack> tt) {
        this((mob, stack) -> tt.test(stack));
    }

    public static TemptingSensor forAnimal() {
        return new TemptingSensor((mob, stack) -> mob instanceof Animal animal && animal.isFood(stack));
    }

    private TemptingSensor(BiPredicate<PathfinderMob, ItemStack> temptations) {
        this.temptations = temptations;
    }

    protected void doTick(ServerLevel level, PathfinderMob body) {
        double range = body.getAttributeValue(Attributes.TEMPT_RANGE);
        List<Player> tempting = new java.util.ArrayList<>();
        for (Object candidate : level.players()) {
            Player player = (Player) candidate;
            if (!player.isSpectator() && body.closerThan(player, range) && isEntityTargetable(level, body, player)
                    && (temptations.test(body, player.getMainHandItem()) || temptations.test(body, player.getOffhandItem()))) {
                tempting.add(player);
            }
        }
        tempting.sort(Comparator.comparingDouble(body::distanceToSqr));
        body.getBrain().setMemory(MemoryModuleType.TEMPTING_PLAYER, tempting.isEmpty() ? null : tempting.get(0));
    }

    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.TEMPTING_PLAYER);
    }
}

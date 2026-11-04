// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.npc.villager.Villager;

public class SecondaryPoiSensor extends Sensor<Villager> {
    private static final int SCAN_RATE = 40;

    public SecondaryPoiSensor() {
        super(SCAN_RATE);
    }

    protected void doTick(ServerLevel level, Villager body) {
        BlockPos center = body.blockPosition();
        List<GlobalPos> sites = new ArrayList<>();
        var secondary = body.getVillagerData().profession().value().secondaryPoi();
        for (int x = -4; x <= 4; x++) {
            for (int y = -2; y <= 2; y++) {
                for (int z = -4; z <= 4; z++) {
                    BlockPos probe = center.offset(x, y, z);
                    if (secondary.contains(level.getBlockState(probe).getBlock())) {
                        sites.add(GlobalPos.of(level.dimension(), probe));
                    }
                }
            }
        }
        if (sites.isEmpty()) {
            body.getBrain().eraseMemory(MemoryModuleType.SECONDARY_JOB_SITE);
        } else {
            body.getBrain().setMemory(MemoryModuleType.SECONDARY_JOB_SITE, sites);
        }
    }

    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.SECONDARY_JOB_SITE);
    }
}

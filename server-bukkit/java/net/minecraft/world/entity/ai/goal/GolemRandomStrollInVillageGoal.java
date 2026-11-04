// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.List;
import java.util.stream.Collectors;
import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.entity.ai.village.poi.PoiRecord;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.phys.Vec3;

public class GolemRandomStrollInVillageGoal extends RandomStrollGoal {
    public GolemRandomStrollInVillageGoal(PathfinderMob mob, double speedModifier) {
        super(mob, speedModifier, 240, false);
    }

    @Override
    protected Vec3 getPosition() {
        net.minecraft.util.RandomSource random = mob.level().getRandom();
        if (random.nextFloat() < 0.3F) {
            return anywhere();
        }
        Vec3 target;
        if (random.nextFloat() < 0.7F) {
            target = towardsVillager();
            if (target == null) {
                target = towardsPoi();
            }
        } else {
            target = towardsPoi();
            if (target == null) {
                target = towardsVillager();
            }
        }
        return target == null ? anywhere() : target;
    }

    private Vec3 anywhere() {
        return LandRandomPos.getPos(mob, 10, 7);
    }

    private Vec3 towardsVillager() {
        ServerLevel level = (ServerLevel) mob.level();
        List<Villager> villagers = level.getEntitiesOfClass(Villager.class, mob.getBoundingBox().inflate(32.0),
                villager -> villager.wantsToSpawnGolem(mob.level().getGameTime()));
        if (villagers.isEmpty()) {
            return null;
        }
        Villager picked = villagers.get(level.getRandom().nextInt(villagers.size()));
        return LandRandomPos.getPosTowards(mob, 10, 7, picked.position());
    }

    private Vec3 towardsPoi() {
        ServerLevel level = (ServerLevel) mob.level();
        List<SectionPos> sections = SectionPos.cube(SectionPos.of(mob.blockPosition()), 2)
                .filter(section -> level.sectionsToVillage(section) == 0).collect(Collectors.toList());
        if (sections.isEmpty()) {
            return null;
        }
        SectionPos section = sections.get(level.getRandom().nextInt(sections.size()));
        List<BlockPos> pois = level.getPoiManager().getInRange(type -> true, section.center(), 8, PoiManager.Occupancy.IS_OCCUPIED)
                .map(PoiRecord::getPos).collect(Collectors.toList());
        if (pois.isEmpty()) {
            return null;
        }
        return LandRandomPos.getPosTowards(mob, 10, 7, Vec3.atBottomCenterOf(pois.get(level.getRandom().nextInt(pois.size()))));
    }
}

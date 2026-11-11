// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.dimension.end;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.boss.enderdragon.EndCrystal;
import net.minecraft.world.level.levelgen.feature.EndSpikeFeature;
import net.minecraft.world.level.Level;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public enum DragonRespawnStage implements StringRepresentable {
    START("start") {
        @Override
        public void tick(ServerLevel level, EnderDragonFight fight, List<EndCrystal> crystals, int time) {
            crystals.forEach(crystal -> crystal.setBeamTarget(kCenter));
            fight.setRespawnStage(PREPARING_TO_SUMMON_PILLARS);
        }
    },
    PREPARING_TO_SUMMON_PILLARS("preparing_to_summon_pillars") {
        @Override
        public void tick(ServerLevel level, EnderDragonFight fight, List<EndCrystal> crystals, int time) {
            if (time >= 100) {
                fight.setRespawnStage(SUMMONING_PILLARS);
            } else if (time == 0 || time == 50 || time == 51 || time == 52 || time >= 95) {
                level.levelEvent(3001, kCenter, 0);
            }
        }
    },
    SUMMONING_PILLARS("summoning_pillars") {
        @Override
        public void tick(ServerLevel level, EnderDragonFight fight, List<EndCrystal> crystals, int time) {
            boolean first = time % 40 == 0;
            boolean last = time % 40 == 39;
            if (!first && !last) {
                return;
            }
            List<EndSpikeFeature.EndSpike> spikes = EndSpikeFeature.getSpikesForLevel(level);
            int index = time / 40;
            if (index >= spikes.size()) {
                if (first) {
                    fight.setRespawnStage(SUMMONING_DRAGON);
                }
                return;
            }
            EndSpikeFeature.EndSpike spike = spikes.get(index);
            if (first) {
                BlockPos top = new BlockPos(spike.getCenterX(), spike.getHeight() + 1, spike.getCenterZ());
                crystals.forEach(crystal -> crystal.setBeamTarget(top));
                return;
            }
            BlockPos low = new BlockPos(spike.getCenterX() - 10, spike.getHeight() - 10, spike.getCenterZ() - 10);
            BlockPos high = new BlockPos(spike.getCenterX() + 10, spike.getHeight() + 10, spike.getCenterZ() + 10);
            for (BlockPos at : BlockPos.betweenClosed(low, high)) {
                level.removeBlock(at, false);
            }
            level.explode(null, spike.getCenterX() + 0.5F, spike.getHeight(), spike.getCenterZ() + 0.5F, 5.0F, Level.ExplosionInteraction.BLOCK);
            new EndSpikeFeature(List.of(spike), true, Optional.of(kCenter)).place(level, level.getChunkSource().getGenerator(), RandomSource.create(),
                new BlockPos(spike.getCenterX(), 45, spike.getCenterZ()));
        }
    },
    SUMMONING_DRAGON("summoning_dragon") {
        @Override
        public void tick(ServerLevel level, EnderDragonFight fight, List<EndCrystal> crystals, int time) {
            if (time >= 100) {
                fight.setRespawnStage(END);
                fight.resetSpikeCrystals();
                for (EndCrystal crystal : crystals) {
                    crystal.setBeamTarget(null);
                    double cx = crystal.getX();
                    level.explode(crystal, cx, crystal.getY(), crystal.getZ(), 6.0F, Level.ExplosionInteraction.NONE);
                    crystal.discard();
                }
            } else if (time >= 80 || (time > 0 && time < 5)) {
                level.levelEvent(3001, kCenter, 0);
            } else if (time == 0) {
                crystals.forEach(crystal -> crystal.setBeamTarget(kCenter));
            }
        }
    },
    END("end") {
        @Override
        public void tick(ServerLevel level, EnderDragonFight fight, List<EndCrystal> crystals, int time) {
        }
    };
    private static final BlockPos kCenter = new BlockPos(0, 128, 0);
    public static final Codec<DragonRespawnStage> CODEC = StringRepresentable.fromEnum(DragonRespawnStage::values);
    private final String name;
    DragonRespawnStage(String name) {
        this.name = name;
    }
    @Override
    public String getSerializedName() {
        return name;
    }
    public abstract void tick(ServerLevel level, EnderDragonFight fight, List<EndCrystal> crystals, int time);
}

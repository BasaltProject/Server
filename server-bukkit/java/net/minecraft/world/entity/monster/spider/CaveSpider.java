// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.spider;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.phys.Vec3;
public class CaveSpider extends Spider {
    public CaveSpider(EntityType<? extends CaveSpider> type, Level level) {
        super(type, level);
    }
    public static AttributeSupplier.Builder createCaveSpider() {
        return Spider.createAttributes().add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 12.0);
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public Vec3 getVehicleAttachmentPoint(Entity vehicle) {
        if (vehicle.getBbWidth() <= getBbWidth()) {
            return new net.minecraft.world.phys.Vec3(0.0, 0.21875 * getScale(), 0.0);
        }
        return super.getVehicleAttachmentPoint(vehicle);
    }
}

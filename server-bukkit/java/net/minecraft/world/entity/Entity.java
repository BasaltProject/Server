package net.minecraft.world.entity;

import java.util.UUID;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.TypedInstance;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.syncher.SyncedDataHolder;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.Nameable;
import net.minecraft.world.level.entity.EntityAccess;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.scores.ScoreHolder;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.craftbukkit.entity.CraftEntity;
import org.bukkit.event.entity.CreatureSpawnEvent;
public abstract class Entity implements Nameable, EntityAccess, ScoreHolder, SyncedDataHolder, DataComponentGetter, ItemOwner, SlotProvider, DebugValueSource, TypedInstance {
    protected Entity() {
    }
    public boolean generation = false;
    public CreatureSpawnEvent.SpawnReason spawnReason;
    public void absSnapTo(double x, double y, double z, float yRot, float xRot) {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.absSnapTo");
    }
    public void discard() {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.discard");
    }
    public CraftEntity getBukkitEntity() {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.getBukkitEntity");
    }
    public Vec3 getPosition(float partialTickTime) {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.getPosition");
    }
    public String getStringUUID() {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.getStringUUID");
    }
    public EntityType getType() {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.getType");
    }
    public UUID getUUID() {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.getUUID");
    }
    public void load(ValueInput input) {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.load");
    }
    public RegistryAccess registryAccess() {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.registryAccess");
    }
    public boolean save(ValueOutput output) {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.save");
    }
    public void saveWithoutId(ValueOutput output) {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.saveWithoutId");
    }
    public void setUUID(UUID uuid) {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.setUUID");
    }
}

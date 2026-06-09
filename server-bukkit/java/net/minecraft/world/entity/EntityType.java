package net.minecraft.world.entity;

import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.entity.EntityTypeTest;
import org.basaltmc.bukkit.EntityBridge;

public class EntityType<T> implements EntityTypeTest, FeatureElement {
    private final Identifier key;

    public EntityType(Identifier key) {
        this.key = key;
    }

    public Entity create(Level level, EntitySpawnReason reason) {
        Entity made = EntityBridge.isAlive(key.toString())
            ? new LivingEntity((EntityType<?>) this, (ServerLevel) level)
            : new Entity((EntityType<?>) this, (ServerLevel) level);
        return made;
    }

    public static Identifier getKey(EntityType type) {
        return type.key;
    }

    public static Entity loadEntityRecursive(EntityType type, CompoundTag tag, Level level, EntitySpawnReason reason, EntityProcessor postLoad) {
        Entity made = type.create(level, reason);
        made.loadTag(tag);
        return postLoad == null ? made : postLoad.process(made);
    }
}

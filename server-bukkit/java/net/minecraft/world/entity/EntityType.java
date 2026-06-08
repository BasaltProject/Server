package net.minecraft.world.entity;

import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.entity.EntityTypeTest;
import org.basaltmc.bukkit.Unsupported;
public class EntityType<T> implements EntityTypeTest, FeatureElement {
    private final Identifier key;

    public EntityType(Identifier key) {
        this.key = key;
    }
    public Entity create(Level level, EntitySpawnReason reason) {
        throw Unsupported.hit("net.minecraft.world.entity.EntityType.create");
    }
    public static Identifier getKey(EntityType type) {
        return type.key;
    }
    public static Entity loadEntityRecursive(EntityType type, CompoundTag tag, Level level, EntitySpawnReason reason, EntityProcessor postLoad) {
        throw Unsupported.hit("net.minecraft.world.entity.EntityType.loadEntityRecursive");
    }
}

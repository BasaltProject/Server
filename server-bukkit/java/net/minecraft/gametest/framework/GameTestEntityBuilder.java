// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.phys.Vec3;

public class GameTestEntityBuilder<E extends Entity> {
    protected final GameTestHelper testHelper;
    protected final EntityType<E> entityType;
    protected final Vec3 position;
    protected EntitySpawnReason spawnReason = EntitySpawnReason.COMMAND;
    protected Rotation rotation = Rotation.NONE;
    protected boolean requirePersistence = true;

    public GameTestEntityBuilder(GameTestHelper testHelper, EntityType<E> entityType, Vec3 position) {
        this.testHelper = testHelper;
        this.entityType = entityType;
        this.position = position;
    }

    public GameTestEntityBuilder<E> spawnReason(EntitySpawnReason spawnReason) {
        this.spawnReason = spawnReason;
        return this;
    }

    public GameTestEntityBuilder<E> rotation(Rotation rotation) {
        this.rotation = rotation;
        return this;
    }

    public GameTestEntityBuilder<E> requirePersistence(boolean requirePersistence) {
        this.requirePersistence = requirePersistence;
        return this;
    }

    protected void prepare(E entity) {
        if (requirePersistence && entity instanceof Mob mob) {
            mob.setPersistenceRequired();
        }
    }

    public E spawn() {
        Vec3 absolute = testHelper.absoluteVec(position);
        E entity = entityType.tryCast(entityType.create(testHelper.getLevel(), spawnReason));
        if (entity == null) {
            throw testHelper.assertionException(BlockPos.containing(position), "test.error.spawn_failure", entityType.builtInRegistryHolder().key().identifier().toString());
        }
        entity.snapTo(absolute.x, absolute.y, absolute.z, testHelper.getAbsoluteDirection(net.minecraft.core.Direction.NORTH).toYRot(), 0.0F);
        prepare(entity);
        testHelper.getLevel().addFreshEntity(entity);
        return entity;
    }

    public List<E> spawn(int amount) {
        List<E> spawned = new ArrayList<>(amount);
        for (int i = 0; i < amount; i++) {
            spawned.add(spawn());
        }
        return spawned;
    }
}
